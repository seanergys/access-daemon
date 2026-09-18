#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <plugins/general/topology_ext.h>
#include <management/cpupow/archs/intel63.c>

typedef struct dev_ctx_s {
    metric_t  met;
    uint8_t   dev_cpupow; // Device of the EAR CPUPOW API
    uint32_t *temporal_values; // EAR uses integers
} dev_ctx_t;

static topology_t      *tp;
static mgt_cpupow_ops_t ops;
static dev_ctx_t        ctxs[CPUPOW_DOMAINS]; // We are using only 2

plug_identity()
{
    *tag       = "act.cpu_pow.msr";
    *tags_deps = "!topology_ext";
}

plug_init(_topology_ext)
{
    int i = 0;
    tp = (topology_t *) data;
    // This is a big class, so I'm invoking the EAR code directly.
    mgt_cpupow_intel63_load(tp, &ops);
    if (ops.powercap_set == NULL) {
        return "[X] not enough privileges to read MSR";
    }
    // At least, the socket powercap should be supported.
    if (!ops.powercap_is_capable(CPUPOW_SOCKET)) {
        return "[X] powercap is not supported";
    }
    // Initializations
    ctxs[CPUPOW_CORE  ].met.device      = Cpu;
    ctxs[CPUPOW_CORE  ].met.device_name = MET_NAME(Cpu);
    ctxs[CPUPOW_CORE  ].met.devs_count  = tp->cpu_count;
    ctxs[CPUPOW_SOCKET].met.device      = Socket;
    ctxs[CPUPOW_SOCKET].met.device_name = MET_NAME(Socket);
    ctxs[CPUPOW_SOCKET].met.devs_count  = tp->socket_count;
    // Allocations
    for (i = 0; i < CPUPOW_DOMAINS; ++i) {
        ctxs[i].met.domain      = Actuator;
        ctxs[i].met.domain_name = MET_NAME(Actuator);
        ctxs[i].met.metric      = Pow;
        ctxs[i].met.metric_name = MET_NAME(Pow);
        ctxs[i].met.units       = "W";
        ctxs[i].met.granularity = ctxs[i].met.device;
        ctxs[i].met.values      = calloc(ctxs[i].met.devs_count, sizeof(uint64_t));
        ctxs[i].temporal_values = calloc(ctxs[i].met.devs_count, sizeof(uint32_t));
        ctxs[i].dev_cpupow      = i;
    }
    return NULL;
}

static void copy_metadata(metric_t *out, metric_t *met, int cpu_min, int cpu_max)
{
    if (cpu_max == 0) { return; }
    memcpy(out, met, sizeof(metric_t));
    out->values       = &met->values[cpu_min];
    out->values_count = cpu_max - cpu_min;
}

static char *get_actuators(void *data, dev_ctx_t *ctx)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t          *inp = &req->input;
    int o       = req->outputs_count;
    int cpu_max = ctx->met.devs_count;
    int cpu_min = 0;
    int cpu     = 0;

    memset(ctx->temporal_values, 0, sizeof(uint)*tp->cpu_count);
    if (inp->device_no != DEVS_ALL) {
        cpu_max = inp->device_no + 1;
        cpu_min = inp->device_no;
    }
    ops.powercap_get(ctx->dev_cpupow, (uint *) ctx->temporal_values);
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        ctx->met.values[cpu] = (uint64_t) ctx->temporal_values[cpu];
    }
    // Returning metric
    copy_metadata(&req->outputs[o], &ctx->met, cpu_min, cpu_max);
    req->outputs_count++;
    return NULL;
}

plug_message_receive(_act_pow_socket_get)
{
    return get_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_pow_cpu_get)
{
    return get_actuators(data, &ctxs[CPUPOW_CORE]);
}

plug_message_receive(_act_pow_all_get)
{
    get_actuators(data, &ctxs[CPUPOW_SOCKET]);
    return get_actuators(data, &ctxs[CPUPOW_CORE]);
}

plug_message_receive(_act_all_all_get)
{
    get_actuators(data, &ctxs[CPUPOW_SOCKET]);
    return get_actuators(data, &ctxs[CPUPOW_CORE]);
}

static char *set_actuators(void *data, dev_ctx_t *ctx)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t          *act = &req->input;
    int cpu_max            = ctx->met.devs_count;
    int cpu_min            = 0;
    int cpu                = 0;

    if (act->device_no >= ctx->met.devs_count || act->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // Fixing the number of values that are coming
    if (act->values_count > ctx->met.devs_count) {
        act->values_count = ctx->met.devs_count;
    }
    // If specific device
    if (act->device_no != DEVS_ALL) {
        cpu_max = act->device_no + 1;
        cpu_min = act->device_no;
    // If received less values than devices
    } else if (act->values_count < ctx->met.devs_count) {
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

plug_message_receive(_act_pow_socket_set)
{
    return set_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_pow_cpu_set)
{
    return set_actuators(data, &ctxs[CPUPOW_CORE]);
}

plug_message_receive(_act_pow_socket_reset)
{
    ops.powercap_reset(ctxs[CPUPOW_SOCKET].dev_cpupow);
    return NULL;
}

plug_message_receive(_act_pow_cpu_reset)
{
    // TODO: the capacity to reset specific devices within a domain
    ops.powercap_reset(ctxs[CPUPOW_CORE].dev_cpupow);
    return NULL;
}

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *out = &req->outputs[req->outputs_count];
    copy_metadata(&out[0], &ctxs[CPUPOW_CORE].met, 0, ctxs[CPUPOW_CORE].met.devs_count);
    copy_metadata(&out[1], &ctxs[CPUPOW_SOCKET].met, 0, ctxs[CPUPOW_SOCKET].met.devs_count);
    req->outputs_count += 2;
    return NULL;
}