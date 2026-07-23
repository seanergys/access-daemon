#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
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

static topology_t *tp;
static uint64_t    base_frequency;
static sensor_t    sensors1[128];
static sensor_t    sensors2[128];
static uint64_t    frequencies[128];

plug_identity()
{
    *tag       = "met.cpu_freq.msr";
    *tags_deps = "!topology_ext";
}

static void get(uint32_t cpu_min, uint32_t cpu_max)
{
    int i;
    for (i = cpu_min; i < cpu_max; ++i) {
        msr_read(tp->cpus[i].id, &sensors2[i].mperf, sizeof(uint64_t), MSR_IA32_MPERF);
        msr_read(tp->cpus[i].id, &sensors2[i].aperf, sizeof(uint64_t), MSR_IA32_APERF);
    }
}

static void diff(uint32_t cpu_min, uint32_t cpu_max)
{
    uint64_t mperf_diff;
    uint64_t aperf_diff;
    uint64_t aperf_pcnt;
    uint32_t i;

    for (i = cpu_min; i < cpu_max; ++i)
    {
        mperf_diff = overflow_magic_u64((uint64_t) sensors2[i].mperf, (uint64_t) sensors1[i].mperf, MAXBITS64);
        aperf_diff = overflow_magic_u64((uint64_t) sensors2[i].aperf, (uint64_t) sensors1[i].aperf, MAXBITS64);
        if (((uint64_t) (-1LU) / 100LU) < aperf_diff) {
            aperf_diff >>= 7;
            mperf_diff >>= 7;
        }
        aperf_pcnt = (aperf_diff * 100LU) / mperf_diff;
        frequencies[i] = (base_frequency * aperf_pcnt) / 100LU;
    }
}

static void copy(uint32_t cpu_min, uint32_t cpu_max)
{
    memcpy(&sensors1[cpu_min], &sensors2[cpu_min], sizeof(sensor_t) * (cpu_max - cpu_min));
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
    uint32_t cpu_max = tp->cpu_count;
    uint32_t cpu_min = 0;
    int o = req->outputs_count;

    // If just one device is specified
    if (req->input.device_no != -1) {
        cpu_max = req->input.device_no + 1;
        cpu_min = req->input.device_no;
    }
    // Computing the metric through sensor values
    get (cpu_min, cpu_max); // Getting new sensor values
    diff(cpu_min, cpu_max); // Making the difference with the previous values
    copy(cpu_min, cpu_max); // Copying the new values to the last ones array
    // Returning metric
    req->outputs[o].device       = Cpu;
    req->outputs[o].device_name  = "cpu";
    req->outputs[o].metric       = Freq;
    req->outputs[o].metric_name  = "freq";
    req->outputs[o].granularity  = Cpu;
    req->outputs[o].values       = &frequencies[cpu_min];
    req->outputs[o].values_count = cpu_max - cpu_min;
    req->outputs[o].units        = "KHz";
    req->outputs_count++;
    return NULL;
}

plug_message_receive(_met_freq_cpu_get)
{
    return return_metrics(data);
}

plug_message_receive(_met_all_all_get)
{
    return return_metrics(data);
}