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
static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;


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
        ctxs[i].met.metric      = Power;
        ctxs[i].met.metric_name = MET_NAME(Power);
        ctxs[i].met.units       = "W";
        ctxs[i].met.granularity = ctxs[i].met.device;
        ctxs[i].met.values      = calloc(ctxs[i].met.devs_count, sizeof(uint64_t));
        ctxs[i].temporal_values = calloc(ctxs[i].met.devs_count, sizeof(uint32_t));
        ctxs[i].dev_cpupow      = i;
    }
    return NULL;
}

static void act_copy(metric_t *out, metric_t *act, uint64_t *copy, int cpu_min, int cpu_max)
{
    if (cpu_max == 0) return;
    memcpy(out, act, sizeof(metric_t)), out->values = NULL; // Meta-data only
    out->values_count = cpu_max - cpu_min; // #Values/devices
    if ((out->values = copy) == NULL) return; // The caller does not want values
    memcpy(copy, &act->values[cpu_min], sizeof(uint64_t)*out->values_count);
}

static char *acts_read(void *data, dev_ctx_t *ctx, uint64_t *copy)
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
    // Reading the values
    while (pthread_mutex_trylock(&lock));
    ops.powercap_get(ctx->dev_cpupow, (uint *) ctx->temporal_values);
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        ctx->met.values[cpu] = (uint64_t) ctx->temporal_values[cpu];
    }
    // Copying metric
    act_copy(&req->outputs[o], &ctx->met, &copy[cpu_min], cpu_min, cpu_max);
    pthread_mutex_unlock(&lock);
    req->outputs_count++;
    return NULL;
}

static char *acts_return(void *data, uint dev)
{
    static __thread uint64_t *copy[CPUPOW_DOMAINS] = {0};
    // Each thread here will have its own copy of values.
    if (copy[dev] == NULL) {
        copy[dev] = calloc(ctxs[dev].met.devs_count, sizeof(uint64_t));
    }
    return acts_read(data, &ctxs[dev], copy[dev]);
}

plug_message_receive(_act_pow_socket_get)
{
    return acts_return(data, CPUPOW_SOCKET);
}

plug_message_receive(_act_pow_cpu_get)
{
    return acts_return(data, CPUPOW_CORE);
}

plug_message_receive(_act_pow_all_get)
{
    acts_return(data, CPUPOW_SOCKET);
    return acts_return(data, CPUPOW_CORE);
}

plug_message_receive(_act_all_all_get)
{
    acts_return(data, CPUPOW_SOCKET);
    return acts_return(data, CPUPOW_CORE);
}

static char *acts_write(void *data, dev_ctx_t *ctx)
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
        printf("ops.powercap_set(%d, %u)\n", cpu, (uint32_t) act->values[cpu]);
    }
    // Disabled because I don't want power changes in my Laptop
    //ops.powercap_set(ctx->dev_cpupow, (uint *) ctx->temporal_values);
    return NULL;
}

plug_message_receive(_act_pow_socket_set)
{
    return acts_write(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_pow_cpu_set)
{
    return acts_write(data, &ctxs[CPUPOW_CORE]);
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
    act_copy(&out[0], &ctxs[CPUPOW_CORE  ].met, NULL, 0, ctxs[CPUPOW_CORE].met.devs_count);
    act_copy(&out[1], &ctxs[CPUPOW_SOCKET].met, NULL, 0, ctxs[CPUPOW_SOCKET].met.devs_count);
    req->outputs_count += 2;
    return NULL;
}