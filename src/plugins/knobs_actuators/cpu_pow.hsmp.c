#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <management/cpupow/archs/amd17.c>

typedef struct dev_ctx_s {
    metric_t      met;
    uint8_t       dev_cpupow; // Device of the EAR CPUPOW API
    uint32_t     *temporal_values; // EAR uses integers
    pthread_key_t copy_key;
} dev_ctx_t;

static topology_t      *tp;
static mgt_cpupow_ops_t ops;
static dev_ctx_t        ctxs[CPUPOW_DOMAINS];
static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;

plug_identity()
{
    *tag       = "act.cpu_pow.hsmp";
    *tags_deps = "!topology_ext";
}

plug_init(_topology_ext)
{
    dev_ctx_t *ctx = &ctxs[CPUPOW_SOCKET];

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
    ctx->met.device       = Socket;
    ctx->met.device_name  = MET_NAME(Socket);
    ctx->met.device_alias = MET_NAME(Pkg);
    ctx->met.devs_count   = tp->socket_count;
    ctx->met.granul       = Socket;
    ctx->met.granul_name  = MET_NAME(Socket);
    ctx->met.domain       = Actuator;
    ctx->met.domain_name  = MET_NAME(Actuator);
    ctx->met.metric       = Power;
    ctx->met.metric_name  = MET_NAME(Power);
    ctx->met.units        = "W";
    ctx->dev_cpupow       = CPUPOW_SOCKET;
    // Allocations
    if (pthread_key_create(&ctx->copy_key, free) != 0) {
        return "[X] Failed to create CPU power output key";
    }
    ctx->met.values      = calloc(ctx->met.devs_count, sizeof(uint64_t));
    ctx->temporal_values = calloc(tp->cpu_count, sizeof(uint32_t));
    if (ctx->met.values == NULL || ctx->temporal_values == NULL) {
        return "[X] memory can't be allocated";
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

static char *get_actuators(void *data, dev_ctx_t *ctx, uint64_t *copy)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *met     = &req->input;
    int       cpu_max = ctx->met.devs_count;
    int       cpu_min = 0;
    int       o       = req->outputs_count;
    int       cpu     = 0;

    if (met->device_no != DEVS_ALL) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
    }
    while (pthread_mutex_trylock(&lock));
    memset(ctx->met.values     , 0, sizeof(uint64_t)*ctx->met.devs_count);
    memset(ctx->temporal_values, 0, sizeof(uint32_t)*ctx->met.devs_count);
    if (state_fail(ops.powercap_get(ctx->dev_cpupow, (uint *) ctx->temporal_values))) {
        ctx->met.state = Error, ctx->met.error_msg = state_msg;
    }
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        ctx->met.values[cpu] = (uint64_t) ctx->temporal_values[cpu];
    }
    // Returning metric
    act_copy(&req->outputs[o], &ctx->met, &copy[cpu_min], cpu_min, cpu_max);
    pthread_mutex_unlock(&lock);
    req->outputs_count++;
    return NULL;
}

static char *acts_return(void *data, uint dev)
{
    dev_ctx_t *ctx = &ctxs[dev];
    uint64_t *copy;
    // Each thread owns its returned values until its next call or exit.
    if ((copy = pthread_getspecific(ctx->copy_key)) == NULL) {
        if ((copy = calloc(ctx->met.devs_count, sizeof(uint64_t))) == NULL) {
            return "[X] memory can't be allocated";
        }
        if (pthread_setspecific(ctx->copy_key, copy) != 0) {
            free(copy);
            return "[X] Failed to store CPU power outputs";
        }
    }
    return get_actuators(data, ctx, copy);
}

plug_message_receive(_act_pow_socket_get)
{
    return acts_return(data, CPUPOW_SOCKET);
}

plug_message_receive(_act_pow_all_get)
{
    return acts_return(data, CPUPOW_SOCKET);
}

plug_message_receive(_act_all_all_get)
{
    return acts_return(data, CPUPOW_SOCKET);
}

static char *set_actuators(void *data, dev_ctx_t *ctx)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *act = &req->input;
    int cpu_max = ctx->met.devs_count;
    int cpu_min = 0;
    int cpu = 0;

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
        if (state_fail(ops.powercap_set(ctx->dev_cpupow, ctx->temporal_values))) {
            req->state = Error, req->error_msg = state_msg;
        }
        return NULL;
    }
    // If received 'all devices', but multiple values, we write them in order
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        ctx->temporal_values[cpu] = (uint32_t) act->values[cpu];
    }
    if (state_fail(ops.powercap_set(ctx->dev_cpupow, (uint *) ctx->temporal_values))) {
        req->state = Error, req->error_msg = state_msg;
    }
    return NULL;
}

plug_message_receive(_act_pow_socket_set)
{
    return set_actuators(data, &ctxs[CPUPOW_SOCKET]);
}

plug_message_receive(_act_pow_socket_reset)
{
    ops.powercap_reset(ctxs[CPUPOW_SOCKET].dev_cpupow);
    return NULL;
}

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *out = &req->outputs[req->outputs_count];

    act_copy(out, &ctxs[CPUPOW_SOCKET].met, NULL, 0,
             ctxs[CPUPOW_SOCKET].met.devs_count);
    req->outputs_count++;
    return NULL;
}
