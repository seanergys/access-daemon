#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/ear_extensions.h>
#include <common/math_operations.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <plugins/general/metrics_central.h>

#define DEVS_MAX 2 // By now just package and pp0
#define DEV_PKG  0
#define DEV_PP0  1

typedef struct dev_ctx_s {
    topology_t  *tp;
    device_t     device;
    const char  *device_name;
    uint64_t     addr_stat;
    uint64_t    *energy;
    uint64_t    *energy_last; // A copy for the diff
    uint64_t    *power; // Space for the immediate/accumulation plugin
    timestamp_t  time;
    timestamp_t  time_last;
} dev_ctx_t;

static dev_ctx_t   ctxs[DEVS_MAX];
static topology_t *tp;
static topology_t *tp_socket;
static uint64_t    power_unit;
static double      units;
static int         accumulator_enabled;

plug_identity()
{
    *tag       = "met.cpu_pow.intel";
    *tags_deps = "!topology_ext";
}

static void get(dev_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    uint64_t sensor;
    uint i;

    if (ctx->addr_stat == 0x000) {
        return;
    }
    timestamp_get(&ctx->time);
    for (i = dev_min; i < dev_max; i++) {
        // TODO: Control failed states
        msr_read(ctx->tp->cpus[i].id, &sensor, sizeof(uint64_t), ctx->addr_stat);
        ctx->energy[i] = (uint64_t) (((double) getbits64(sensor, 31, 0)) * units);
    }
}

static void imm(dev_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    double secs = 0.0;
    double diff = 0.0;
    uint i;

    // Seconds since last all. Is only required to convert from energy
    // accumulation to immediate power value.
    if (timestamp_is_zero(&ctx->time_last)) {
        return;
    }
    secs = timestamp_fdiff(&ctx->time, &ctx->time_last, TIME_SECS, TIME_MSECS);
    // Given this is an accumulative register, as is the energy concept, we have
    // to make the difference between the current sample and the last one, and
    // divide by the seconds passed to get the power value.
    for (i = dev_min; i < dev_max; i++) {
        diff = (double) (ctx->energy[i] - ctx->energy_last[i]);
        ctx->power[i] = (uint64_t) ceil(diff / secs);
    }
}

static void copy(dev_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    // Saving last values to convert to immediate
    memcpy(&ctx->energy_last[dev_min], &ctx->energy[dev_min], sizeof(uint64_t) * (dev_max - dev_min));
    ctx->time_last = ctx->time;
}

plug_init(_topology_ext)
{
    static topology_t tp_socket_st;
    uint64_t res;
    int cpu, d;

    tp = (topology_t *) data;
    #define ADDR_INIT(pu, pkg, dram, pp0, pp1) { \
        power_unit  = pu;   \
        ctxs[DEV_PKG].addr_stat = pkg; \
        ctxs[DEV_PP0].addr_stat = pp0; \
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
    ctxs[DEV_PKG].tp = tp_socket;
    ctxs[DEV_PP0].tp = tp;
    ctxs[DEV_PKG].device = Socket;
    ctxs[DEV_PP0].device = Cpu;
    ctxs[DEV_PKG].device_name = MET_NAME(Socket);
    ctxs[DEV_PP0].device_name = MET_NAME(Cpu);
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
        ctxs[d].energy      = calloc(tp->cpu_count, sizeof(uint64_t));
        ctxs[d].energy_last = calloc(tp->cpu_count, sizeof(uint64_t));
        ctxs[d].power       = calloc(tp->cpu_count, sizeof(uint64_t)); // Space for immediate
        get (&ctxs[d], 0, ctxs[d].tp->cpu_count);
        copy(&ctxs[d], 0, ctxs[d].tp->cpu_count);
    }
    return NULL;
}

plug_init(_metrics_accumulator)
{
    // The accumulator plugin is present so we can get accumulative values
    accumulator_enabled = 1;
}

static char *return_metrics(void *data, dev_ctx_t *ctx)
{
    metrics_call_t *req = (metrics_call_t *) data;
    metrics_output_t  *out = &req->outputs[req->outputs_count];
    metrics_input_t   *met = &req->input;
    uint32_t     immediate = (met->command == GetAcc);
    uint32_t       cpu_max = ctx->tp->cpu_count;
    uint32_t       cpu_min = 0;

    if (met->device_no >= ctx->tp->cpu_count || met->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // If just one device is specified
    if (met->device_no != DEVS_ALL) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
    }
    // Given the difference is only required to get the immediate value, and the
    // power is only offered when accumulation plugin is enabled, which only
    // asks for all devices in regular periods of time and does not let others
    // from reading values, this plugin does not have potential dangers.
    get (ctx, cpu_min, cpu_max);
    imm (ctx, cpu_min, cpu_max);
    copy(ctx, cpu_min, cpu_max);
    // Returning metric
    out[0].device       = ctx->device;
    out[0].device_name  = ctx->device_name;
    out[0].metric       = Energy;
    out[0].metric_name  = MET_NAME(Energy);
    out[0].time         = ctx->time;
    out[0].values       = &ctx->energy[cpu_min];
    out[0].values_count = cpu_max - cpu_min;
    out[0].units        = "J";
    out[0].granularity  = ctx->device;
    if (immediate) {
    out[1].device       = ctx->device;
    out[1].device_name  = ctx->device_name;
    out[1].metric       = Pow;
    out[1].metric_name  = MET_NAME(Pow);
    out[1].time         = ctx->time;
    out[1].values       = &ctx->power[cpu_min];
    out[1].values_count = cpu_max - cpu_min;
    out[1].units        = "W";
    out[1].granularity  = ctx->device;
    }
    req->outputs_count += 1 + immediate;
    return NULL;
}

plug_message_receive(_met_energy_cpu_get)
{
    return return_metrics(data, &ctxs[DEV_PP0]);
}

plug_message_receive(_met_energy_socket_get)
{
    return return_metrics(data, &ctxs[DEV_PKG]);
}

plug_message_receive(_met_energy_all_get)
{
    return_metrics(data, &ctxs[DEV_PP0]);
    return return_metrics(data, &ctxs[DEV_PKG]);
}

plug_message_receive(_met_all_all_get)
{
    return_metrics(data, &ctxs[DEV_PP0]);
    return return_metrics(data, &ctxs[DEV_PKG]);
}
