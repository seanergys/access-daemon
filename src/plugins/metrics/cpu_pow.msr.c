#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/ear_extensions.h>
#include <common/math_operations.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <common/metrics_types.h>

#define DEVS_MAX 2 // By now just package and pp0
#define DEV_PKG  0
#define DEV_PP0  1
#define METS_MAX 2
#define E        0 // Index 0 is Energy
#define P        1 // Index 1 is Power (derived)

typedef struct dev_ctx_s {
    topology_t    *tp;
    uint64_t       addr_stat;
    mets_outs_t    metrics_c[METS_MAX]; // Current
    mets_outs_t    metrics_l[METS_MAX]; // Last
} dev_ctx_t;

static dev_ctx_t   devs[DEVS_MAX]; // Devices meta-data and values
static topology_t *tp;
static topology_t *tp_socket;
static uint64_t    power_unit;
static double      units;
static int         acc_on; // Accumulator plugin is online

plug_identity()
{
    *tag       = "met.cpu_pow.intel";
    *tags_deps = "!topology_ext";
}

static void get(dev_ctx_t *dev, uint32_t dev_min, uint32_t dev_max)
{
    uint64_t sensor;
    uint i;

    if (dev->addr_stat == 0x000) {
        return;
    }
    timestamp_get(&dev->metrics_c[E].time);
    for (i = dev_min; i < dev_max; i++) {
        // TODO: Control failed states
        msr_read(dev->tp->cpus[i].id, &sensor, sizeof(uint64_t), dev->addr_stat);
        dev->metrics_c[E].values[i] = (uint64_t) (((double) getbits64(sensor, 31, 0)) * units);
    }
}

static void imm(dev_ctx_t *dev, uint32_t dev_min, uint32_t dev_max)
{
    double secs = 0.0;
    double diff = 0.0;
    uint i;

    // Seconds since last all. Is only required to convert from energy
    // accumulation to immediate power value.
    if (timestamp_is_zero(&dev->metrics_l[E].time)) {
        return;
    }
    secs = timestamp_fdiff(&dev->metrics_c[E].time, &dev->metrics_l[E].time, TIME_SECS, TIME_MSECS);
    // Given this is an accumulative register, as is the energy concept, we have
    // to make the difference between the current sample and the last one, and
    // divide by the seconds passed to get the power value.
    for (i = dev_min; i < dev_max; i++) {
        diff = (double) (dev->metrics_c[E].values[i] - dev->metrics_l[E].values[i]);
        dev->metrics_c[P].values[i] = (uint64_t) ceil(diff / secs);
    }
}

static void copy(dev_ctx_t *dev, uint32_t dev_min, uint32_t dev_max)
{
    // Saving last values to convert to immediate
    memcpy(&dev->metrics_l[E].values[dev_min], &dev->metrics_c[E].values[dev_min], sizeof(uint64_t) * (dev_max - dev_min));
    dev->metrics_l[E].time = dev->metrics_c[E].time;
}

plug_init(_topology_ext)
{
    static topology_t tp_socket_st;
    int cpu, d, m;
    uint64_t res;

    tp = (topology_t *) data;
    #define ADDR_INIT(pu, pkg, dram, pp0, pp1) { \
        power_unit  = pu;   \
        devs[DEV_PKG].addr_stat = pkg; \
        devs[DEV_PP0].addr_stat = pp0; \
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
    devs[DEV_PKG].tp = tp_socket;
    devs[DEV_PP0].tp = tp;
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
    if ((state_fail(msr_read(tp->cpus[E].id, &res, sizeof(res), power_unit)))) {
        return "[X] failed while reading MSRs";
    }
    units = pow(0.5, (double) getbits64(res, 12, 8));
    // Allocatiion and first readings
    for (m = 0; m < METS_MAX; ++m) {
        devs[DEV_PKG].metrics_c[m].device      = Socket;
        devs[DEV_PP0].metrics_c[m].device      = Cpu;
        devs[DEV_PKG].metrics_c[m].device_name = MET_NAME(Socket);
        devs[DEV_PP0].metrics_c[m].device_name = MET_NAME(Cpu);
    }
    for (d = 0; d < DEVS_MAX; ++d) {
        devs[d].metrics_c[E].values      = calloc(tp->cpu_count, sizeof(uint64_t));
        devs[d].metrics_c[P].values      = calloc(tp->cpu_count, sizeof(uint64_t)); // Space for immediate
        devs[d].metrics_c[E].metric      = Energy;
        devs[d].metrics_c[P].metric      = Power;
        devs[d].metrics_c[E].metric_name = MET_NAME(Energy);
        devs[d].metrics_c[P].metric_name = MET_NAME(Power);
        devs[d].metrics_c[E].units       = "J";
        devs[d].metrics_c[P].units       = "P";
        devs[d].metrics_l[E].values      = calloc(tp->cpu_count, sizeof(uint64_t));
        get (&devs[d], 0, devs[d].tp->cpu_count);
        copy(&devs[d], 0, devs[d].tp->cpu_count);
    }
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

static char *return_metrics(void *data, dev_ctx_t *dev)
{
    metrics_call_t    *req = (metrics_call_t *) data;
    metrics_output_t  *out = &req->outputs[req->outputs_count];
    metrics_input_t   *inp = &req->input;
    uint32_t     immediate = (inp->command == GetAcc);
    uint32_t       cpu_max = dev->tp->cpu_count;
    uint32_t       cpu_min = 0;

    if (inp->device_no >= dev->tp->cpu_count || inp->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // If just one device is specified
    if (inp->device_no != All) {
        cpu_max = inp->device_no + 1;
        cpu_min = inp->device_no;
    }
    // Given the difference is only required to get the immediate value, and the
    // power is only offered when accumulation plugin is enabled, which only
    // asks for all devices in regular periods of time and does not let others
    // from reading values, this plugin does not have potential dangers.
    get (dev, cpu_min, cpu_max);
    imm (dev, cpu_min, cpu_max);
    copy(dev, cpu_min, cpu_max);
    // Returning metric
    copy_metadata(&out[E], &dev->metrics_c[E], cpu_min, cpu_max);
    copy_metadata(&out[P], &dev->metrics_c[P], cpu_min, cpu_max * immediate);
    req->outputs_count += 1 + immediate;
    return NULL;
}

plug_message_receive(_met_energy_cpu_get)
{
    return return_metrics(data, &devs[DEV_PP0]);
}

plug_message_receive(_met_energy_socket_get)
{
    return return_metrics(data, &devs[DEV_PKG]);
}

plug_message_receive(_met_energy_all_get)
{
    return_metrics(data, &devs[DEV_PP0]);
    return return_metrics(data, &devs[DEV_PKG]);
}

plug_message_receive(_met_all_all_get)
{
    return_metrics(data, &devs[DEV_PP0]);
    return return_metrics(data, &devs[DEV_PKG]);
}

plug_message_receive(_met_all_all_info)
{
    metrics_call_t   *req = (metrics_call_t *) data;
    metrics_output_t *out = &req->outputs[req->outputs_count];
    copy_metadata(&out[0], &devs[DEV_PP0].metrics_c[E], 0, devs[DEV_PP0].tp->cpu_count);
    copy_metadata(&out[1], &devs[DEV_PKG].metrics_c[E], 0, devs[DEV_PKG].tp->cpu_count);
    copy_metadata(&out[2], &devs[DEV_PP0].metrics_c[P], 0, devs[DEV_PP0].tp->cpu_count * acc_on);
    copy_metadata(&out[3], &devs[DEV_PKG].metrics_c[P], 0, devs[DEV_PKG].tp->cpu_count * acc_on);
    req->outputs_count += 2 + (acc_on*2);
    return NULL;
}