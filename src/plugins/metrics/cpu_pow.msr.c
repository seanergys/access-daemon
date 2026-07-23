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

typedef struct dev_ctx_s {
    topology_t  *tp;
    device_t     device;
    const char  *device_name;
    uint64_t     addr_stat;
    uint64_t    *sensors1;
    uint64_t    *sensors2;
    uint64_t    *power;
    timestamp_t  sensors1_time;
    timestamp_t  sensors2_time;
} dev_ctx_t;

static dev_ctx_t   ctxs[DEVS_MAX];
static topology_t *tp;
static topology_t *tp_socket;
static uint64_t    power_unit;
static double      units;

plug_identity()
{
    *tag       = "met.cpu_pow.intel";
    *tags_deps = "!topology_ext";
}

static void get(dev_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    uint i;

    if (ctx->addr_stat == 0x000) {
        return;
    }
    timestamp_get(&ctx->sensors2_time);
    for (i = dev_min; i < dev_max; i++) {
        // TODO: Control failed states
        msr_read(ctx->tp->cpus[i].id, &ctx->sensors2[i], sizeof(uint64_t), ctx->addr_stat);
        ctx->sensors2[i] = getbits64(ctx->sensors2[i], 31, 0);
    }
}

static void diff(dev_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    double ctx__power_d;
    double secs;
    uint32_t i;

    if (ctx->addr_stat == 0x000) {
        return;
    }
    secs = timestamp_fdiff(&ctx->sensors2_time, &ctx->sensors1_time, TIME_SECS, TIME_MSECS);
    for (i = dev_min; i < dev_max; i++) {
        ctx->power[i] = overflow_magic_u64(ctx->sensors2[i], ctx->sensors1[i], MAXBITS32);
        ctx__power_d  = (((double) ctx->power[i]) * units) / secs;
        ctx->power[i] = (uint64_t) ctx__power_d;
    }
}

static void copy(dev_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    ctx->sensors1_time = ctx->sensors2_time;
    memcpy(&ctx->sensors1[dev_min], &ctx->sensors2[dev_min], sizeof(uint64_t)*(dev_max-dev_min));
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
    ctxs[DEV_PKG].device_name = "socket";
    ctxs[DEV_PP0].device_name = "cpu";
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
        ctxs[d].sensors1 = calloc(tp->cpu_count, sizeof(uint64_t));
        ctxs[d].sensors2 = calloc(tp->cpu_count, sizeof(uint64_t));
        ctxs[d].power    = calloc(tp->cpu_count, sizeof(uint64_t));
        get (&ctxs[d], 0, ctxs[d].tp->cpu_count);
        copy(&ctxs[d], 0, ctxs[d].tp->cpu_count);
    }
    return NULL;
}

static char *return_metrics(void *data, dev_ctx_t *ctx)
{
    request_metrics_t *req = (request_metrics_t *) data;
    metrics_input_t   *met = &req->input;
    uint32_t cpu_max = ctx->tp->cpu_count;
    uint32_t cpu_min = 0000000;
    int o = req->outputs_count;

    if (met->device_no >= ctx->tp->cpu_count || met->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // If just one device is specified
    if (req->input.device_no != DEVS_ALL) {
        cpu_max = req->input.device_no + 1;
        cpu_min = req->input.device_no;
    }
    // This is key behaviour:
    // - get() function has to read all devices. All counters must be
    //   synchronized if there will be periodic calls from the Telemetry
    //   Collector.
    // - This could be problematic if too many components are consulting some
    //   metric values. But is easy to solve. We can just measure the time
    //   passed and cover the get-diff-copy functions with a conditional that
    //   tests if the time passed is greater or equal than one or half second.
    //   If not, we can just immediately return the last readings.
    get (ctx, 0, ctx->tp->cpu_count);
    diff(ctx, 0, ctx->tp->cpu_count);
    copy(ctx, 0, ctx->tp->cpu_count); // Copying the new values to the last ones array
    // Returning metric
    req->outputs[o].device       = ctx->device;
    req->outputs[o].device_name  = ctx->device_name;
    req->outputs[o].metric       = Pow;
    req->outputs[o].metric_name  = "pow";
    req->outputs[o].granularity  = ctx->device;
    req->outputs[o].values       = ctx->power;
    req->outputs[o].values_count = cpu_max - cpu_min;
    req->outputs[o].units        = "W";
    req->outputs_count++;
    return NULL;
}

plug_message_receive(_met_pow_cpu_get)
{
    return return_metrics(data, &ctxs[DEV_PP0]);
}

plug_message_receive(_met_pow_socket_get)
{
    return return_metrics(data, &ctxs[DEV_PKG]);
}

plug_message_receive(_met_pow_all_get)
{
    return_metrics(data, &ctxs[DEV_PP0]);
    return return_metrics(data, &ctxs[DEV_PKG]);
}

plug_message_receive(_met_all_all_get)
{
    return_metrics(data, &ctxs[DEV_PP0]);
    return return_metrics(data, &ctxs[DEV_PKG]);
}