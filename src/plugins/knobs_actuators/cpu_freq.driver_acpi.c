/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <common/framework.h>
#include <common/utils/keeper.h>
#include <common/metrics_types.h>
#include <metrics/common/file.h>
#include <plugins/general/topology_ext.h>
#include <plugins/knobs_actuators/cpu_freq.driver_governor.h>

#define PATH_SSS  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed"

static topology_t     *tp;
static int            *fds_sss;
static metric_t        act;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static char            default_frequency[64];

plug_identity()
{
    *tag       = "act.cpu_freq.driver_acpi";
    *tags_deps = "!topology_ext,!act.cpu_freq.driver_governor";
}

plug_init(_topology_ext)
{
    tp = (topology_t *) data;
    // Testing if frequency/governor set files are accessible
    int set_frequency = filemagic_can_mwrite(PATH_SSS, tp->cpu_count, &fds_sss);
    if (!set_frequency) return rsprintf("[X] frequencies can not be set: %s", state_msg);
    if (!cpu_freq_is_userspace_available()) return "[X] Userspace governor not available";
    // Saving default frequency
    if (!keeper_load_text("CpuFreqDefault", default_frequency)) {
        if (filemagic_word_read(fds_sss[0], default_frequency, 1)) {
            keeper_save_text("CpuFreqDefault", default_frequency);
        }
    }
    // Initializations
    act.domain      = Actuator;
    act.domain_name = MET_NAME(Actuator);
    act.device      = Cpu;
    act.device_name = MET_NAME(Cpu);
    act.devs_count  = tp->cpu_count;
    act.metric      = Freq;
    act.metric_name = MET_NAME(Freq);
    act.values      = calloc(act.devs_count, sizeof(uint64_t));
    act.units       = "KHz";
    act.granul      = act.device;
    act.granul_name = act.device_name;
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

static char *acts_read(void *data, uint64_t *copy)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t   *met = &req->input;
    int o          = req->outputs_count;
    char value[64] = {0};
    int cpu_max    = tp->cpu_count;
    int cpu_min    = 0;
    int cpu        = 0;

    if (met->device_no != -1) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
    }
    // Reading the values
    while (pthread_mutex_trylock(&lock));
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        act.values[cpu] = 0LLU;
        if (filemagic_word_read(fds_sss[cpu], value, 1)) {
            act.values[cpu] = (uint64_t) atoll(value);
        }
    }
    // Copying metric
    act_copy(&req->outputs[o], &act, &copy[cpu_min], cpu_min, cpu_max);
    pthread_mutex_unlock(&lock);
    req->outputs_count++;
    return NULL;
}

static char *acts_return(void *data)
{
    static __thread uint64_t *copy = NULL;
    // Each thread here will have its own copy of values.
    if (copy == NULL) {
        copy = calloc(act.devs_count, sizeof(uint64_t));
    }
    return acts_read(data, copy);
}

plug_message_receive(_act_freq_cpu_get)
{
    return acts_return(data);
}

plug_message_receive(_act_all_all_get)
{
    return acts_return(data);
}

plug_message_receive(_act_freq_cpu_set)
{
    char freq_str[16]; // Enough for a frequency in Hz
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t   *act = &req->input;
    uint32_t cpu;

    if (act->device_no >= tp->cpu_count || act->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // Fixing values count
    if (act->values_count > tp->cpu_count) {
        act->values_count = tp->cpu_count;
    }
    // If received 'all devices' but just one actuator value, it means all the
    // actuators should be set to the same value.
    if (act->device == -1 && act->values_count == 1) {
        // TODO: validate if the frequency is in the range
        cpu_freq_governor_set(-1, Governor.userspace);
        sprintf(freq_str, "%lu", act->values[0]);
        filemagic_word_mwrite(fds_sss, tp->cpu_count, freq_str, 1);
        return NULL;
    }
    // Preparing the per-device iteration
    if (act->device == -1) {
        act->values_count = tp->cpu_count;
        act->device = 0;
    } else {
        act->values_count = act->device + 1;
    }
    // If all devices, but multiple values, we write them in order
    for (cpu = act->device; cpu < act->values_count; ++cpu) {
        // TODO: validate if the frequency is in the range
        sprintf(freq_str, "%lu", act->values[cpu]);
        cpu_freq_governor_set(cpu, Governor.userspace);
        filemagic_word_write(fds_sss[cpu], freq_str, strlen(freq_str), 1);
    }
    return NULL;
}

plug_message_receive(_act_freq_cpu_reset)
{
    if (strlen(default_frequency) > 0 && strstr(default_frequency, "unsup") != NULL) {
        filemagic_word_mwrite(fds_sss, tp->cpu_count, default_frequency, 1);
    }
    cpu_freq_governor_reset(-1);
    return NULL;
}

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *out = &req->outputs[req->outputs_count];
    act_copy(out, &act, NULL, 0, act.devs_count);
    req->outputs_count++;
    return NULL;
}