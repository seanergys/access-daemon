#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <management/cpupow/archs/amd17.c>

typedef struct dev_ctx_s {
    device_t     dev;
    const char  *dev_name;
    uint8_t      dev_cpupow; // Device of the EAR CPUPOW API
    uint32_t     devs_count;
    uint64_t    *current_knobs;
    uint32_t    *temporal_values; // EAR uses integers
} dev_ctx_t;

static topology_t *tp;
static mgt_cpupow_ops_t ops;
static dev_ctx_t ctxs[CPUPOW_DOMAINS]; // We are using only 2

plug_identity()
{
    *tag       = "act.cpu_pow.hsmp";
    *tags_deps = "!topology_ext";
}

plug_init(_topology_ext)
{
    int i = 0;
    tp = (topology_t *) data;
    // This is a big class, so I'm invoking the EAR code directly.
    mgt_cpupow_amd17_load(tp, &ops);
    if (ops.powercap_set == NULL) {
        return "[X] not enough privileges to read MSR";
    }
    // At least, the socket powercap should be supported.
    if (!ops.powercap_is_capable(CPUPOW_SOCKET)) {
        return "[X] powercap is not supported";
    }
    // Initializations
    ctxs[CPUPOW_SOCKET].devs_count = tp->socket_count;
    ctxs[CPUPOW_SOCKET].dev        = Socket;
    ctxs[CPUPOW_SOCKET].dev_name   = MET_NAME(Socket);
    ctxs[CPUPOW_CORE  ].devs_count = tp->cpu_count;
    ctxs[CPUPOW_CORE  ].dev        = Cpu;
    ctxs[CPUPOW_CORE  ].dev_name   = MET_NAME(Cpu);
    // Allocations
    for (i = 0; i < CPUPOW_DOMAINS; ++i) {
        ctxs[i].dev_cpupow      = i;
        ctxs[i].current_knobs   = calloc(tp->cpu_count, sizeof(uint64_t));
        ctxs[i].temporal_values = calloc(tp->cpu_count, sizeof(uint32_t));
    }
    return NULL;
}

static char *get_actuators(void *data, dev_ctx_t *ctx)
{
    metrics_call_t *req = (metrics_call_t *) data;
    metrics_input_t   *met = &req->input;
    int o       = req->outputs_count;
    int cpu_max = ctx->devs_count;
    int cpu_min = 0;
    int cpu     = 0;

    memset(ctx->temporal_values, 0, sizeof(uint)*tp->cpu_count);
    if (met->device_no != DEVS_ALL) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
    }
    ops.powercap_get(ctx->dev_cpupow, (uint *) ctx->temporal_values);
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        ctx->current_knobs[cpu] = (uint64_t) ctx->temporal_values[cpu];
    }
    // Returning metric
    req->outputs[o].device       = ctx->dev;
    req->outputs[o].device_name  = ctx->dev_name;
    req->outputs[o].metric       = Pow;
    req->outputs[o].metric_name  = MET_NAME(Pow);
    req->outputs[o].values       = &ctx->current_knobs[cpu_min];
    req->outputs[o].units        = "W";
    req->outputs[o].values_count = cpu_max - cpu_min;
    req->outputs[o].granularity  = ctx->dev;
    req->outputs_count++;
    return NULL;
}

plug_message_receive(_act_pow_cpu_get)
{
    return get_actuators(data, &ctxs[CPUPOW_CORE]);
}

plug_message_receive(_act_pow_socket_get)
{
    return get_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_pow_all_get)
{
    get_actuators(data, &ctxs[CPUPOW_CORE]);
    return get_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_all_all_get)
{
    get_actuators(data, &ctxs[CPUPOW_CORE]);
    return get_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

static char *set_actuators(void *data, dev_ctx_t *ctx)
{
    metrics_call_t *req    = (metrics_call_t *) data;
    metrics_input_t   *act    = &req->input;
    int cpu_max               = ctx->devs_count;
    int cpu_min               = 0;
    int cpu                   = 0;

    if (act->device_no >= ctx->devs_count || act->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // Fixing the number of values that are coming
    if (act->values_count > ctx->devs_count) {
        act->values_count = ctx->devs_count;
    }
    // If specific device
    if (act->device_no != DEVS_ALL) {
        cpu_max = act->device_no + 1;
        cpu_min = act->device_no;
    // If received less values than devices
    } else if (act->values_count < ctx->devs_count) {
        cpu_max = act->values_count + 1;
    }
    // Setting default values
    for (cpu = 0; cpu < cpu_max; ++cpu) {
        ctx->temporal_values[cpu] = POWERCAP_DO_NOTHING;
    }
    // If received 'all devices' but just one actuator value, it means all the
    // actuators should be set to the same value.
    if (act->device_no == DEVS_ALL && act->values_count == 1) {
        for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
            ctx->temporal_values[cpu] = (uint32_t) act->values[0];
        }
        ops.powercap_set(ctx->dev_cpupow, ctx->temporal_values);
        return NULL;
    }
    // If received 'all devices', but multiple values, we write them in order
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        ctx->temporal_values[cpu] = (uint32_t) act->values[cpu];
    }
    ops.powercap_set(ctx->dev_cpupow, (uint *) ctx->temporal_values);
    return NULL;
}

plug_message_receive(_act_pow_cpu_set)
{
    return set_actuators(data, &ctxs[CPUPOW_CORE]);
}

plug_message_receive(_act_pow_socket_set)
{
    return set_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_pow_cpu_reset)
{
    // TODO: the capacity to reset specific devices within a domain
    ops.powercap_reset(ctxs[CPUPOW_CORE].dev_cpupow);
    return NULL;
}

plug_message_receive(_act_pow_socket_reset)
{
    ops.powercap_reset(ctxs[CPUPOW_SOCKET].dev_cpupow);
    return NULL;
}