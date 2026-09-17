#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <common/ear_extensions.h>
#include <common/math_operations.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <plugins/general/topology_ext.h>

#define MSR_IA32_APERF 0x000000E8
#define MSR_IA32_MPERF 0x000000E7

#define METS_MAX 3
#define F        0 // Index 0 is current Freq
#define A        1 // Index 1 is current AccFreq (derived)
#define LF       2 // Index 2 is last Freq

typedef struct sensor_s {
    uint64_t aperf;
    uint64_t mperf;
} sensor_t;

// TODO: allocate space based on number of cpus
static topology_t *tp;
static uint64_t    base_frequency;
static sensor_t   *registers1;
static sensor_t   *registers2;
static mets_outs_t mets[METS_MAX]; // Current and last Freq and AccFreq
static int         acc_on; // Accumulator plugin is online

plug_identity()
{
    *tag       = "met.cpu_freq.msr";
    *tags_deps = "!topology_ext";
}

static void get(uint32_t cpu_min, uint32_t cpu_max)
{
    int i;

    timestamp_get(&mets[F].time);
    for (i = cpu_min; i < cpu_max; ++i) {
        msr_read(tp->cpus[i].id, &registers2[i].mperf, sizeof(uint64_t), MSR_IA32_MPERF);
        msr_read(tp->cpus[i].id, &registers2[i].aperf, sizeof(uint64_t), MSR_IA32_APERF);
    }
}

static void acc(uint32_t cpu_min, uint32_t cpu_max)
{
    uint64_t aperf_percent = 0LLU;
    uint64_t mperf_diff    = 0LLU;
    uint64_t aperf_diff    = 0LLU;
    double frequency       = 0.0;
    double seconds         = 0.0;
    int i;

    if (timestamp_is_zero(&mets[LF].time)) {
        return;
    }
    seconds = timestamp_fdiff(&mets[F].time, &mets[LF].time, TIME_SECS, TIME_MSECS);
    //
    for (i = cpu_min; i < cpu_max; ++i) {
        // APERF and MPERF registers are deltas. This means time doesn't belong
        // to this calculus: after making a difference you will get the average
        // frequency between two specific times. But to accumulate these clocks,
        // the time turns a requirement because if the accumulation happens
        // every two seconds, you will have to multiplicate by 2.
        mperf_diff = overflow_magic_u64((uint64_t) registers2[i].mperf, (uint64_t) registers1[i].mperf, MAXBITS64);
        aperf_diff = overflow_magic_u64((uint64_t) registers2[i].aperf, (uint64_t) registers1[i].aperf, MAXBITS64);
        if (((uint64_t) (-1LU) / 100LU) < aperf_diff) {
            aperf_diff >>= 7;
            mperf_diff >>= 7;
        }
        aperf_percent      = (aperf_diff * 100LU) / mperf_diff;
        mets[F].values[i]  = (base_frequency * aperf_percent) / 100LU;
        frequency          = (double) mets[F].values[i];
        mets[A].values[i] += (uint64_t) (frequency * seconds);
    }
}

static void copy(uint32_t cpu_min, uint32_t cpu_max)
{
    memcpy(&registers1[cpu_min], &registers2[cpu_min], sizeof(sensor_t) * (cpu_max - cpu_min));
    mets[LF].time = mets[F].time;
}

plug_init(_topology_ext)
{
    int cpu, m;

    tp = (topology_t *) data;
    if ((tp->vendor == VENDOR_INTEL && tp->model  >= MODEL_HASWELL_X) ||
        (tp->vendor == VENDOR_AMD   && tp->family >= FAMILY_ZEN    )) {
        // Compatible
    } else {
        return "[X] not compatible with this CPU";
    }
    base_frequency = ((topology_ext_t *) data)->base_frequency;
    // Testing if MSR can be read
    if (state_fail(msr_test(tp, MSR_RD))) {
        return "[X] not enough privileges to read MSR";
    }
    // Opening MSRs.
    for (cpu = 0; cpu < tp->cpu_count; ++cpu) {
        if (state_fail(msr_open(tp->cpus[cpu].id, MSR_RD))) {
            for (cpu -= 1; cpu >= 0; --cpu) {
                msr_close(tp->cpus[cpu].id);
            }
            return "[X] failed while opening MSRs";
        }
    }
    // Allocating and meta-data
    mets[F].metric      = Freq;
    mets[F].metric_name = MET_NAME(Freq);
    mets[A].metric      = AccFreq;
    mets[A].metric_name = MET_NAME(AccFreq);
    for (m = 0; m < METS_MAX; ++m) {
        mets[m].device      = Cpu;
        mets[m].device_name = MET_NAME(Cpu);
        mets[m].values      = calloc(tp->cpu_count, sizeof(uint64_t));
        mets[m].units       = "KHz";
        mets[m].granularity = Cpu;
    }
    // APERF and MPERF values don't fit in the metrics
    registers1 = calloc(tp->cpu_count, sizeof(sensor_t));
    registers2 = calloc(tp->cpu_count, sizeof(sensor_t));
    // First readings
    get (0, tp->cpu_count);
    copy(0, tp->cpu_count);
    return NULL;
}

plug_init(_metrics_accumulator)
{
    // The accumulator plugin is present so we can get accumulative values
    acc_on = 1;
    return NULL;
}

static void copy_metadata(mets_outs_t *out, mets_outs_t *met, int cpu_min, int cpu_max)
{
    if (cpu_max == 0) { return; }
    memcpy(out, met, sizeof(mets_outs_t));
    out->values       = &met->values[cpu_min];
    out->values_count = cpu_max - cpu_min;
}

static char *return_metrics(void *data)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metrics_output_t  *out = &req->outputs[req->outputs_count];
    uint32_t   accumulated = (req->input.command == GetAcc);
    uint32_t       cpu_max = tp->cpu_count;
    uint32_t       cpu_min = 0;
    // If just one device is specified
    if (req->input.device_no != All) {
        cpu_max = req->input.device_no + 1;
        cpu_min = req->input.device_no;
    }
    // Computing the metric through sensor values. Potential dangers:
    //   - When someone calls to often, the differences tighten up, and when
    //     this happens, the frequency average that is calculated after
    //     processing the APERF and MPERF may be less precise.
    get (cpu_min, cpu_max); // Getting new sensor values
    acc (cpu_min, cpu_max); // The accumulation is only possible if
    copy(cpu_min, cpu_max); // Copying the new values for later.
    // Returning metric
    copy_metadata(&out[F], &mets[F], cpu_min, cpu_max);
    copy_metadata(&out[A], &mets[A], cpu_min, cpu_max * accumulated);
    req->outputs_count += 1 + accumulated;
    return NULL;
}

plug_message_receive(_met_freq_cpu_get)
{
    // APERF and MPERF values are not understandable as accumulative frequency.
    // After processing them you only get the average frequency between two
    // specific times, and this means is converted to an immediate value.
    return return_metrics(data);
}

plug_message_receive(_met_all_all_get)
{
    return return_metrics(data);
}

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metrics_output_t  *out = &req->outputs[req->outputs_count];
    copy_metadata(&out[F], &mets[F], 0, tp->cpu_count);
    copy_metadata(&out[A], &mets[A], 0, tp->cpu_count * acc_on);
    req->outputs_count += 1 + acc_on;
    return NULL;
}