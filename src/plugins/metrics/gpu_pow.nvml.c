#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <stdlib.h>
#include <common/framework.h>
#include <metrics/common/nvml.h>
#include <common/metrics_types.h>

#define CTX_MEM    0
#define CTX_SM     1
#define CTX_POWER  2
#define CTX_ENERGY 3
#define CTXS_MAX   4

typedef struct metric_ctx_s {
    uint index;
    metric_t metric;
    const char *name;
    metric_t derived_metric;
    const char *derived_name;
    const char *units;
    const char *derived_units;
    uint64_t *values;
    uint64_t *derived;
    double *total; // Preserve fractions when integrating instantaneous readings.
    timestamp_t *time_last; // Separate baselines for single-device requests.
    timestamp_t time;
} metric_ctx_t;

static metric_ctx_t ctxs[CTXS_MAX] = {
    { .index = CTX_MEM, .metric = FreqMem, .name = MET_NAME(FreqMem),
      .derived_metric = AccFreqMem, .derived_name = MET_NAME(AccFreqMem),
      .units = "KHz", .derived_units = "KHz" },
    { .index = CTX_SM, .metric = Freq, .name = MET_NAME(Freq),
      .derived_metric = AccFreq, .derived_name = MET_NAME(AccFreq),
      .units = "KHz", .derived_units = "KHz" },
    { .index = CTX_POWER, .metric = Pow, .name = MET_NAME(Pow),
      .units = "W" },
    { .index = CTX_ENERGY, .metric = Energy, .name = MET_NAME(Energy),
      .units = "J" },
};
static nvml_t nvml;
static nvmlDevice_t *handlers;
static uint handlers_count;
static void *energy_library;
static nvmlReturn_t (*GetTotalEnergyConsumption)(nvmlDevice_t, unsigned long long *);

plug_identity()
{
    *tag = "met.gpu_pow.nvml";
    *tags_deps = "!topology_ext";
}

static char *get(metric_ctx_t *ctx, uint32_t dev_min, uint32_t dev_max)
{
    uint i, sensor;
    unsigned long long energy;
    uint64_t raw;
    double secs;
    nvmlReturn_t status;

    timestamp_get(&ctx->time);
    for (i = dev_min; i < dev_max; ++i) {
        if (ctx->index == CTX_MEM || ctx->index == CTX_SM) {
            status = nvml.GetClocks(handlers[i],
                ctx->index == CTX_MEM ? NVML_CLOCK_MEM : NVML_CLOCK_SM, &sensor);
            if (status != NVML_SUCCESS) return "[X] NVML clock read failed";
            raw = (uint64_t) sensor * 1000LU;
        } else if (ctx->index == CTX_ENERGY) {
            if (!GetTotalEnergyConsumption) return "[X] NVML energy not available";
            status = GetTotalEnergyConsumption(handlers[i], &energy);
            if (status != NVML_SUCCESS) return "[X] NVML energy read failed";
            raw = energy / 1000LU; // mJ to J.
        } else {
            status = nvml.GetPowerUsage(handlers[i], &sensor);
            if (status != NVML_SUCCESS) return "[X] NVML power read failed";
            raw = sensor / 1000LU; // mW to W.
        }
        ctx->values[i] = raw;
        if (ctx->derived_name) {
            secs = 0.0;
            if (ctx->time_last[i].tv_sec != 0 || ctx->time_last[i].tv_nsec != 0) {
                secs = timestamp_fdiff(&ctx->time, &ctx->time_last[i], TIME_SECS, TIME_MSECS);
            }
            if (secs > 0.0) ctx->total[i] += (double) raw * secs;
            ctx->derived[i] = (uint64_t) ctx->total[i];
            ctx->time_last[i] = ctx->time;
        }
    }
    return NULL;
}

static void release(void)
{
    uint c;
    for (c = 0; c < CTXS_MAX; ++c) {
        free(ctxs[c].values);
        free(ctxs[c].derived);
        free(ctxs[c].total);
        free(ctxs[c].time_last);
    }
    free(handlers);
    handlers = NULL;
    handlers_count = 0;
    if (energy_library) dlclose(energy_library);
    energy_library = NULL;
    GetTotalEnergyConsumption = NULL;
}

plug_init(_topology_ext)
{
    Dl_info library;
    uint c, i;

    if (state_fail(nvml_open(&nvml))) return "[X] NVML failed to open";
    nvml_get_handlers(&handlers, &handlers_count);
    if (!handlers || !handlers_count) {
        release();
        return "[X] No NVML devices";
    }
    // EAR loads NVML locally. Resolve the optional symbol from that same library
    // without changing the external nvml_t ABI.
    if (dladdr((void *) nvml.GetPowerUsage, &library)) {
        energy_library = dlopen(library.dli_fname, RTLD_NOW | RTLD_LOCAL);
        if (energy_library) {
            GetTotalEnergyConsumption = dlsym(energy_library, "nvmlDeviceGetTotalEnergyConsumption");
        }
    }
    for (c = 0; c < CTXS_MAX; ++c) {
        metric_ctx_t *ctx = &ctxs[c];
        ctx->values = calloc(handlers_count, sizeof(*ctx->values));
        if (ctx->derived_name) {
            ctx->derived = calloc(handlers_count, sizeof(*ctx->derived));
            ctx->total = calloc(handlers_count, sizeof(*ctx->total));
            ctx->time_last = calloc(handlers_count, sizeof(*ctx->time_last));
        }
        if (!ctx->values || (ctx->derived_name &&
            (!ctx->derived || !ctx->total || !ctx->time_last))) {
            release();
            return "[X] NVML allocation failed";
        }
        // Unsupported metrics must not prevent other contexts from loading.
        for (i = 0; i < handlers_count; ++i) get(ctx, i, i + 1);
    }
    return NULL;
}

static char *return_metrics(void *data, metric_ctx_t *ctx, int derived)
{
    metrics_call_t *req = data;
    metrics_output_t *out;
    uint32_t dev_min = 0, dev_max = handlers_count;
    char *error;
    uint j;

    derived = derived && ctx->derived_name != NULL;
    if (req->input.device_no < DEVS_ALL ||
        (req->input.device_no != DEVS_ALL && (uint32_t) req->input.device_no >= handlers_count)) {
        error = "[X] Invalid device";
    } else if (!handlers_count) {
        error = "[X] No NVML devices";
    } else if (req->outputs_count + 1 + derived > MAX_METRICS_NO) {
        error = "[X] Too many metrics";
    } else {
        if (req->input.device_no != DEVS_ALL) {
            dev_min = req->input.device_no;
            dev_max = dev_min + 1;
        }
        error = get(ctx, dev_min, dev_max);
    }
    if (error) {
        req->state = Error;
        req->error_msg = error;
        return error;
    }
    out = &req->outputs[req->outputs_count];
    for (j = 0; j < 1U + (uint) derived; ++j) {
        out[j] = (metrics_output_t) {
            .state = Ok, .device = Gpu, .device_name = MET_NAME(Gpu),
            .device_no = req->input.device_no,
            .metric = j ? ctx->derived_metric : ctx->metric,
            .metric_name = j ? ctx->derived_name : ctx->name,
            .time = ctx->time,
            .values = &(j ? ctx->derived : ctx->values)[dev_min],
            .values_count = dev_max - dev_min,
            .units = j ? ctx->derived_units : ctx->units, .granularity = Gpu,
        };
    }
    req->outputs_count += 1 + derived;
    return NULL;
}

static char *return_one(void *data, uint index)
{
    metrics_call_t *req = data;
    return return_metrics(data, &ctxs[index], req->input.command == GetAcc);
}

static char *return_all(void *data)
{
    metrics_call_t *req = data;
    char *error;
    uint c;
    for (c = 0; c < CTXS_MAX; ++c) {
        error = return_metrics(data, &ctxs[c], req->input.command == GetAcc);
        if (error) return error;
    }
    return NULL;
}

plug_message_receive(_met_freqmem_gpu_get) { return return_one(data, CTX_MEM); }
plug_message_receive(_met_freqmem_all_get) { return return_one(data, CTX_MEM); }
plug_message_receive(_met_freq_gpu_get)    { return return_one(data, CTX_SM); }
plug_message_receive(_met_freq_all_get)    { return return_one(data, CTX_SM); }
plug_message_receive(_met_pow_gpu_get)     { return return_one(data, CTX_POWER); }
plug_message_receive(_met_pow_all_get)     { return return_one(data, CTX_POWER); }
plug_message_receive(_met_energy_gpu_get)  { return return_one(data, CTX_ENERGY); }
plug_message_receive(_met_energy_all_get)  { return return_one(data, CTX_ENERGY); }
plug_message_receive(_met_all_gpu_get)     { return return_all(data); }
plug_message_receive(_met_all_all_get)     { return return_all(data); }
