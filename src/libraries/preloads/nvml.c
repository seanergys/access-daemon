/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <metrics/common/nvml.h>

#ifndef CUDA_BASE
#define NVML_ERROR_UNKNOWN 999
#endif

#define GPUS_MAX 4
#define ENERGY_BASE_MJ 1000000ULL
#define PRELOAD_EXPORT __attribute__((visibility("default")))

typedef struct sim_gpu_s {
    uint32_t clock_mhz;
    uint32_t power_mw;
} sim_gpu_t;

static sim_gpu_t       gpus[GPUS_MAX] = {
    { 1500, 100000 },
    { 1600, 125000 },
    { 1700, 150000 },
    { 1800, 175000 },
};
static struct timespec start;
static uint32_t        opens;

static nvmlDevice_t device_handle(uint32_t index)
{
#ifdef CUDA_BASE
    return (nvmlDevice_t) &gpus[index];
#else
    return (nvmlDevice_t) index;
#endif
}

static sim_gpu_t *get_gpu(nvmlDevice_t device)
{
    uint32_t i;

    for (i = 0; i < GPUS_MAX; ++i) {
        if (device == device_handle(i)) return &gpus[i];
    }
    return NULL;
}

static nvmlReturn_t get_clocks(nvmlDevice_t device, nvmlClockType_t type, uint *clock)
{
    sim_gpu_t *gpu;

    if (!opens) return NVML_ERROR_UNINITIALIZED;
    if (!clock || (type != NVML_CLOCK_MEM && type != NVML_CLOCK_SM)) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }
    if ((gpu = get_gpu(device)) == NULL) return NVML_ERROR_INVALID_ARGUMENT;
    *clock = gpu->clock_mhz;
    return NVML_SUCCESS;
}

static nvmlReturn_t get_power(nvmlDevice_t device, uint *power)
{
    sim_gpu_t *gpu;

    if (!opens) return NVML_ERROR_UNINITIALIZED;
    if (!power) return NVML_ERROR_INVALID_ARGUMENT;
    if ((gpu = get_gpu(device)) == NULL) return NVML_ERROR_INVALID_ARGUMENT;
    *power = gpu->power_mw;
    return NVML_SUCCESS;
}

PRELOAD_EXPORT state_t nvml_open(nvml_t *nvml)
{
    static const nvml_t api = {
        .GetPowerUsage = get_power,
        .GetClocks     = get_clocks,
    };

    if (!opens && clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        return_msg(EAR_ERROR, "NVML simulator clock failed");
    }
    ++opens;
    if (nvml) *nvml = api;
    return EAR_SUCCESS;
}

PRELOAD_EXPORT state_t nvml_close(void)
{
    if (opens) --opens;
    return EAR_SUCCESS;
}

PRELOAD_EXPORT void nvml_get_handlers(nvmlDevice_t **handlers, uint *count)
{
    uint32_t devs = opens ? GPUS_MAX : 0;
    nvmlDevice_t *copy;
    uint32_t i;

    if (count) *count = devs;
    if (!handlers) return;
    *handlers = NULL;
    if (!devs) return;
    if ((copy = calloc(devs, sizeof(*copy))) == NULL) {
        if (count) *count = 0;
        return;
    }
    for (i = 0; i < devs; ++i) copy[i] = device_handle(i);
    *handlers = copy;
}

// The plugin resolves this symbol from the library containing GetPowerUsage.
// EAR's nvml_t does not contain a total-energy callback.
PRELOAD_EXPORT nvmlReturn_t nvmlDeviceGetTotalEnergyConsumption(nvmlDevice_t device,
                                                              unsigned long long *energy)
{
    struct timespec now;
    double secs;
    sim_gpu_t *gpu;

    if (!opens) return NVML_ERROR_UNINITIALIZED;
    if (!energy) return NVML_ERROR_INVALID_ARGUMENT;
    if ((gpu = get_gpu(device)) == NULL) return NVML_ERROR_INVALID_ARGUMENT;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return NVML_ERROR_UNKNOWN;
    secs = now.tv_sec - start.tv_sec + (now.tv_nsec - start.tv_nsec) / 1e9;
    *energy = ENERGY_BASE_MJ + (unsigned long long) (secs * gpu->power_mw);
    return NVML_SUCCESS;
}
