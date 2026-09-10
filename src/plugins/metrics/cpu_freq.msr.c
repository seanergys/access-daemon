#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/ear_extensions.h>
#include <common/math_operations.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <plugins/general/metrics_central.h>

#define MSR_IA32_APERF 0x000000E8
#define MSR_IA32_MPERF 0x000000E7

typedef struct sensor_s {
    uint64_t aperf;
    uint64_t mperf;
} sensor_t;

// TODO: allocate space based on number of cpus
static topology_t *tp;
static uint64_t    base_frequency;
static sensor_t    registers1[128];
static sensor_t    registers2[128];
static uint64_t    accfreqs[128];
static uint64_t    immfreqs[128];
static timestamp_t time2;
static timestamp_t time1;

plug_identity()
{
    *tag       = "met.cpu_freq.msr";
    *tags_deps = "!topology_ext";
}

static void get(uint32_t cpu_min, uint32_t cpu_max)
{
    int i;

    timestamp_get(&time2);
    for (i = cpu_min; i < cpu_max; ++i) {
        msr_read(tp->cpus[i].id, &registers2[i].mperf, sizeof(uint64_t), MSR_IA32_MPERF);
        msr_read(tp->cpus[i].id, &registers2[i].aperf, sizeof(uint64_t), MSR_IA32_APERF);
    }
}

static void acc(uint32_t cpu_min, uint32_t cpu_max)
{
    uint64_t mperf_diff;
    uint64_t aperf_diff;
    uint64_t aperf_pcnt;
    double freq = 0.0;
    double secs = 0.0;
    uint32_t i;

    if (timestamp_is_zero(&time1)) {
        return;
    }
    secs = timestamp_fdiff(&time2, &time1, TIME_SECS, TIME_MSECS);
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
        aperf_pcnt   = (aperf_diff * 100LU) / mperf_diff;
        immfreqs[i]  = (base_frequency * aperf_pcnt) / 100LU;
        freq         = (double) immfreqs[cpu_min];
        accfreqs[i] += (uint64_t) (freq * secs);
    }
}

static void copy(uint32_t cpu_min, uint32_t cpu_max)
{
    memcpy(&registers1[cpu_min], &registers2[cpu_min], sizeof(sensor_t) * (cpu_max - cpu_min));
    time1 = time2;
}

plug_init(_topology_ext)
{
    int cpu;

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
    get (0, tp->cpu_count);
    copy(0, tp->cpu_count);
    return NULL;
}

static char *return_metrics(void *data)
{
    request_metrics_t *req = (request_metrics_t *) data;
    metrics_output_t  *out = &req->outputs[req->outputs_count];
    uint32_t   accumulated = (req->input.command == GetAcc);
    uint32_t       cpu_max = tp->cpu_count;
    uint32_t       cpu_min = 0;

    // If just one device is specified
    if (req->input.device_no != -1) {
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
    out[0].device        = Cpu;
    out[0].device_name   = MET_NAME(Cpu);
    out[0].metric        = Freq;
    out[0].metric_name   = MET_NAME(Freq);
    out[0].values        = &immfreqs[cpu_min];
    out[0].values_count  = cpu_max - cpu_min;
    out[0].units         = "KHz";
    out[0].granularity   = Cpu;
    if (accumulated) {
    out[1].device        = Cpu;
    out[1].device_name   = MET_NAME(Cpu);
    out[1].metric        = AccFreq;
    out[1].metric_name   = MET_NAME(AccFreq);
    out[1].values        = &accfreqs[cpu_min];
    out[1].values_count  = cpu_max - cpu_min;
    out[1].units         = "KHz";
    out[1].granularity   = Cpu;
    }
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