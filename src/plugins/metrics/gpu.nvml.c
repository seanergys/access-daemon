/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <common/system/time.h>
#include <common/metrics_types.h>
#include <metrics/common/nvml.h>

#define MET_MEM    0
#define MET_SM     1
#define MET_POWER  2
#define MET_ENERGY 3
#define METS_REAL  4
#define METS_ACC   2
#define METS_MAX   (METS_REAL + METS_ACC)

static metric_t      mets[METS_MAX] = {
    { .metric = FreqMem,    .metric_name = MET_NAME(FreqMem),    .units = "KHz" },
    { .metric = Freq,       .metric_name = MET_NAME(Freq),       .units = "KHz" },
    { .metric = Pow,        .metric_name = MET_NAME(Pow),        .units = "W"   },
    { .metric = Energy,     .metric_name = MET_NAME(Energy),     .units = "J"   },
    { .metric = AccFreqMem, .metric_name = MET_NAME(AccFreqMem), .units = "KHz" },
    { .metric = AccFreq,    .metric_name = MET_NAME(AccFreq),    .units = "KHz" },
};
static nvmlReturn_t (*get_total_energy)(nvmlDevice_t, unsigned long long *);
static double       *total[METS_ACC]; // Preserve fractions when integrating instantaneous readings.
static void         *energy_library;
static nvmlDevice_t *handlers;
static uint          handlers_count;
static uint64_t     *samples;
static uint32_t      acc_on;
static nvml_t        nvml;

plug_identity()
{
    *tag       = "met.gpu.nvml";
    *tags_deps = NULL;
}

static char *read_value(metric_e metric, nvmlDevice_t device, uint64_t *value)
{
    unsigned long long raw_energy;
    nvmlClockType_t clock_type;
    uint raw;

    switch (metric) {
    case FreqMem:
    case Freq:
        clock_type = metric == FreqMem ? NVML_CLOCK_MEM : NVML_CLOCK_SM;
        if (nvml.GetClocks(device, clock_type, &raw) != NVML_SUCCESS) {
            return "[X] NVML clock read failed";
        }
        *value = (uint64_t) raw * 1000LU; // MHz to KHz.
        return NULL;
    case Energy:
        if (!get_total_energy) {
            return "[X] NVML energy not available";
        }
        if (get_total_energy(device, &raw_energy) != NVML_SUCCESS) {
            return "[X] NVML energy read failed";
        }
        *value = raw_energy / 1000LU; // mJ to J.
        return NULL;
    case Pow:
        if (nvml.GetPowerUsage(device, &raw) != NVML_SUCCESS) {
            return "[X] NVML power read failed";
        }
        *value = raw / 1000LU; // mW to W.
        return NULL;
    default:
        return "[X] Unsupported NVML metric";
    }
}

static char *dev_get(uint32_t index, uint32_t dev_min, uint32_t dev_max,
                     timestamp_t *time)
{
    metric_t *met = &mets[index];
    uint32_t dev;
    char *error;

    timestamp_get(time);
    for (dev = dev_min; dev < dev_max; ++dev) {
        if ((error = read_value(met->metric, handlers[dev], &samples[dev]))) {
            return error;
        }
    }
    return NULL;
}

static void dev_acc(uint32_t index, timestamp_t *time)
{
    double secs = 0.0;
    metric_t *acc;
    uint32_t dev;

    if (!acc_on || index >= METS_ACC) {
        return;
    }
    acc = &mets[METS_REAL + index];
    if (acc->time.tv_sec != 0 || acc->time.tv_nsec != 0) {
        secs = timestamp_fdiff(time, &acc->time, TIME_SECS, TIME_MSECS);
    }
    for (dev = 0; dev < handlers_count; ++dev) {
        if (secs > 0.0) {
            total[index][dev] += (double) samples[dev] * secs;
        }
        acc->values[dev] = (uint64_t) total[index][dev];
    }
}

static void dev_copy(uint32_t index, uint32_t dev_min, uint32_t dev_max,
                     timestamp_t *time)
{
    metric_t *met = &mets[index];

    memcpy(&met->values[dev_min], &samples[dev_min],
           (dev_max - dev_min) * sizeof(*samples));
    met->time = *time;
    if (acc_on && index < METS_ACC) {
        mets[METS_REAL + index].time = *time;
    }
}

static void release(void)
{
    uint c;

    for (c = 0; c < METS_MAX; ++c) {
        free(mets[c].values);
    }
    for (c = 0; c < METS_ACC; ++c) {
        free(total[c]);
    }
    free(samples);
    samples = NULL;
    free(handlers);
    handlers = NULL;
    handlers_count = 0;
    if (energy_library) dlclose(energy_library);
    energy_library = NULL;
    get_total_energy = NULL;
}

static char *alloc_metric(metric_t *met)
{
    met->device       = Gpu;
    met->device_name  = MET_NAME(Gpu);
    met->granul       = Gpu;
    met->granul_name  = MET_NAME(Gpu);
    met->domain       = Metric;
    met->domain_name  = MET_NAME(Metric);
    met->values_count = handlers_count;
    if ((met->values = calloc(handlers_count, sizeof(*met->values))) == NULL) {
        return "[X] NVML allocation failed";
    }
    return NULL;
}

plug_init()
{
    timestamp_t time;
    Dl_info library;
    char *error;
    uint c;

    if (state_fail(nvml_open(&nvml))) {
        return "[X] NVML failed to open";
    }
    nvml_get_handlers(&handlers, &handlers_count);
    if (!handlers || !handlers_count) {
        release();
        return "[X] No NVML devices";
    }
    if ((samples = calloc(handlers_count, sizeof(*samples))) == NULL) {
        release();
        return "[X] NVML allocation failed";
    }
    // EAR loads NVML locally. Resolve the optional symbol from that same library
    // without changing the external nvml_t ABI.
    if (dladdr((void *) nvml.GetPowerUsage, &library)) {
        if ((energy_library = dlopen(library.dli_fname, RTLD_NOW | RTLD_LOCAL))) {
            get_total_energy = dlsym(energy_library, "nvmlDeviceGetTotalEnergyConsumption");
        }
    }
    for (c = 0; c < METS_MAX; ++c) {
        if ((error = alloc_metric(&mets[c])) != NULL) {
            release();
            return error;
        }
        if (c < METS_ACC) {
            if (!(total[c] = calloc(handlers_count, sizeof(*total[c])))) {
                release();
                return "[X] NVML allocation failed";
            }
        }
    }
    for (c = 0; c < METS_REAL; ++c) {
        // Unsupported metrics must not prevent other metrics from loading.
        if ((error = dev_get(c, 0, handlers_count, &time))) {
            continue;
        }
        dev_acc (c, &time);
        dev_copy(c, 0, handlers_count, &time);
    }
    return NULL;
}

plug_init(_metrics_accumulator)
{
    acc_on = 1;
    return NULL;
}

static char *return_metrics(void *data, uint32_t index, int accumulated)
{
    uint32_t           dev_max = handlers_count;
    metrics_request_t *req     = data;
    uint32_t           dev_min = 0;
    uint32_t           read_min;
    uint32_t           read_max;
    timestamp_t        time;
    metric_t          *out;
    char              *error;
    uint               j;

    accumulated = accumulated && acc_on && index < METS_ACC;
    if (req->input.device_no < DEVS_ALL ||
        (req->input.device_no != DEVS_ALL && (uint32_t) req->input.device_no >= handlers_count)) {
        error = "[X] Invalid device";
    } else if (!handlers_count) {
        error = "[X] No NVML devices";
    } else if (req->outputs_count + 1 + accumulated > MAX_METRICS_NO) {
        error = "[X] Too many metrics";
    } else {
        if (req->input.device_no != DEVS_ALL) {
            dev_min = req->input.device_no;
            dev_max = dev_min + 1;
        }
        read_min = dev_min;
        read_max = dev_max;
        // A shared accumulation baseline requires a complete device batch.
        if (acc_on && index < METS_ACC) {
            read_min = 0;
            read_max = handlers_count;
        }
        error = dev_get(index, read_min, read_max, &time);
    }
    if (error) {
        req->state = Error;
        req->error_msg = error;
        return error;
    }
    dev_acc (index, &time);
    dev_copy(index, read_min, read_max, &time);
    out = &req->outputs[req->outputs_count];
    for (j = 0; j < 1U + (uint) accumulated; ++j) {
        out[j] = j ? mets[METS_REAL + index] : mets[index];
        out[j].device_no = req->input.device_no;
        out[j].values += dev_min;
        out[j].values_count = dev_max - dev_min;
    }
    req->outputs_count += 1 + accumulated;
    return NULL;
}

static char *return_one(void *data, uint32_t index)
{
    metrics_request_t *req = data;

    return return_metrics(data, index, req->input.command == GetAcc);
}

static char *return_all(void *data)
{
    metrics_request_t *req = data;
    char *error;
    uint c;

    for (c = 0; c < METS_REAL; ++c) {
        if ((error = return_metrics(data, c, req->input.command == GetAcc))) {
            return error;
        }
    }
    return NULL;
}

plug_message_receive(_met_freqmem_gpu_get) { return return_one(data, MET_MEM); }
plug_message_receive(_met_freqmem_all_get) { return return_one(data, MET_MEM); }
plug_message_receive(_met_freq_gpu_get)    { return return_one(data, MET_SM); }
plug_message_receive(_met_freq_all_get)    { return return_one(data, MET_SM); }
plug_message_receive(_met_pow_gpu_get)     { return return_one(data, MET_POWER); }
plug_message_receive(_met_pow_all_get)     { return return_one(data, MET_POWER); }
plug_message_receive(_met_energy_gpu_get)  { return return_one(data, MET_ENERGY); }
plug_message_receive(_met_energy_all_get)  { return return_one(data, MET_ENERGY); }
plug_message_receive(_met_all_gpu_get)     { return return_all(data); }
plug_message_receive(_met_all_all_get)     { return return_all(data); }

plug_message_receive(_all_all_all_info)
{
    uint32_t count = METS_REAL + acc_on * METS_ACC;
    metrics_request_t *req = data;
    metric_t *out;
    char *error;
    uint i;

    if (req->outputs_count + count > MAX_METRICS_NO) {
        error = "[X] Too many metrics";
        req->state = Error;
        req->error_msg = error;
        return error;
    }
    out = &req->outputs[req->outputs_count];
    for (i = 0; i < count; ++i) {
        out[i] = mets[i];
        out[i].values = NULL;
        out[i].device_no = DEVS_ALL;
    }
    req->outputs_count += count;
    return NULL;
}
