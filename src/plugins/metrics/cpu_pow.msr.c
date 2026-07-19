#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/math_operations.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <plugins/general/metrics_central.h>

#define DEVS_MAX 2 // By now just package and pp0
#define DEV_PKG  0
#define DEV_PP0  1

typedef struct values_s {
    topology_t  *tp;
    device_t     device;
    const char  *device_name;
    uint64_t     addr_stat;
    uint64_t    *sensors1;
    uint64_t    *sensors2;
    uint64_t    *power;
    timestamp_t  sensors1_time;
    timestamp_t  sensors2_time;
} values_t;

static values_t    values[DEVS_MAX];
static topology_t *tp;
static topology_t *tp_socket;
static uint64_t    power_unit;
static double      units;

plug_identity()
{
    *tag       = "met.cpu_pow.intel";
    *tags_deps = "!topology_ext";
}

static void get(values_t *vals, uint32_t dev_min, uint32_t dev_max)
{
    uint i;

    if (vals->addr_stat == 0x000) {
        return;
    }
    timestamp_get(&vals->sensors2_time);
    for (i = dev_min; i < dev_max; i++) {
        msr_read(vals->tp->cpus[i].id, &vals->sensors2[i], sizeof(uint64_t), vals->addr_stat);
        vals->sensors2[i] = getbits64(vals->sensors2[i], 31, 0);
    }
}

static void diff(values_t *vals, uint32_t dev_min, uint32_t dev_max)
{
    double vals__power_d;
    double secs;
    uint32_t i;

    if (vals->addr_stat == 0x000) {
        return;
    }
    secs = timestamp_fdiff(&vals->sensors2_time, &vals->sensors1_time, TIME_SECS, TIME_MSECS);
    for (i = dev_min; i < dev_max; i++) {
        vals->power[i] = overflow_magic_u64(vals->sensors2[i], vals->sensors1[i], MAXBITS32);
        vals__power_d  = (((double) vals->power[i]) * units) / secs;
        vals->power[i] = (uint64_t) vals__power_d;
    }
}

static void copy(values_t *vals, uint32_t dev_min, uint32_t dev_max)
{
    vals->sensors1_time = vals->sensors2_time;
    memcpy(&vals->sensors1[dev_min], &vals->sensors2[dev_min], sizeof(uint64_t)*(dev_max-dev_min));
}

plug_init(_topology_ext)
{
    static topology_t tp_socket_st;
    uint64_t res;
    int cpu, d;

    tp = (topology_t *) data;
    #define ADDR_INIT(pu, pkg, dram, pp0, pp1) { \
        power_unit  = pu;   \
        values[DEV_PKG].addr_stat = pkg; \
        values[DEV_PP0].addr_stat = pp0; \
    }
    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_TIGERLAKE) {
        ADDR_INIT(0x00000606, 0x00000611, 0x619, 0x00000639, 0x641);
    } else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
        ADDR_INIT(0xc0010299, 0xc001029B, 0x000, 0xc001029A, 0x000);
    } else {
        return "[X] not compatible with this CPU";
    }
    // Getting also the topology per socket
    topology_select(tp, &tp_socket_st, TPSelect.socket, TPGroup.merge, 0);
    tp_socket = &tp_socket_st;
    // More initializations
    values[DEV_PKG].tp = tp_socket;
    values[DEV_PP0].tp = tp;
    values[DEV_PKG].device = Socket;
    values[DEV_PP0].device = Cpu;
    values[DEV_PKG].device_name = "socket";
    values[DEV_PP0].device_name = "cpu";
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
    if ((state_fail(msr_read(tp->cpus[0].id, &res, sizeof(res), power_unit)))) {
        return "[X] failed while reading MSRs";
    }
    units = pow(0.5, (double) getbits64(res, 12, 8));
    // Allocatiion and first readings
    for (d = 0; d < DEVS_MAX; ++d) {
        values[d].sensors1 = calloc(tp->cpu_count, sizeof(uint64_t));
        values[d].sensors2 = calloc(tp->cpu_count, sizeof(uint64_t));
        values[d].power    = calloc(tp->cpu_count, sizeof(uint64_t));
        get (&values[d], 0, values[d].tp->cpu_count);
        copy(&values[d], 0, values[d].tp->cpu_count);
    }
    return NULL;
}

static char *return_metrics(void *data, values_t *vals)
{
    request_metrics_t *req = (request_metrics_t *) data;
    uint32_t cpu_max = vals->tp->cpu_count;
    uint32_t cpu_min = 0000000;
    int o = req->outputs_count;

    // TODO: Device number test
    // If just one device is specified
    if (req->input.device_no != -1) {
        cpu_max = req->input.device_no + 1;
        cpu_min = req->input.device_no;
    }
    // Computing the metric through sensor values
    get (vals, cpu_min, cpu_max); // Getting new sensor values
    diff(vals, cpu_min, cpu_max); // Making the difference with the previous values
    copy(vals, cpu_min, cpu_max); // Copying the new values to the last ones array
    // Returning metric
    req->outputs[o].device       = vals->device;
    req->outputs[o].device_name  = vals->device_name;
    req->outputs[o].metric       = Pow;
    req->outputs[o].metric_name  = "pow";
    req->outputs[o].granularity  = vals->device;
    req->outputs[o].values       = vals->power;
    req->outputs[o].values_count = cpu_max - cpu_min;
    req->outputs[o].units        = "W";
    req->outputs_count++;
    return NULL;
}

plug_message_receive(_met_cpu_pow_get)
{
    return return_metrics(data, &values[DEV_PP0]);
}

plug_message_receive(_met_socket_pow_get)
{
    return return_metrics(data, &values[DEV_PKG]);
}

plug_message_receive(_met_all_all_get)
{
    return_metrics(data, &values[DEV_PP0]);
    return return_metrics(data, &values[DEV_PKG]);
}