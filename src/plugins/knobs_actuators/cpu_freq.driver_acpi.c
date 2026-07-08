#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <common/utils/keeper.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>
#include <plugins/knobs_actuators/cpu_freq.driver_governor.h>

#define PATH_SSS  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed"

static topology_t *tp;
static int        *fds_sgv;
static int        *fds_sss;
static uint64_t   *current_list;
static char        default_frequency[64];

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
    current_list = calloc(tp->cpu_count, sizeof(uint64_t));
    // Saving default frequency
    if (!keeper_load_text("CpuFreqDefault", default_frequency)) {
        if (filemagic_word_read(fds_sss[0], default_frequency, 1)) {
            keeper_save_text("CpuFreqDefault", default_frequency);
        }
    }
    return NULL;
}

static char *return_actuators_values(void *data)
{
    char value[64] = {0};
    met_t *met  = (met_t *) data;
    int cpu_max = tp->cpu_count;
    int cpu_min = 0;
    int cpu     = 0;

    if (met->device != -1) {
        cpu_max = met->device + 1;
        cpu_min = met->device;
    }
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        current_list[cpu] = 0LLU;
        if (filemagic_word_read(fds_sss[cpu], value, 1)) {
            current_list[cpu] = (uint64_t) atoll(value);
        }
    }
    // Returning metric
    met->metric_name  = "cpu_freq";
    met->values       = &current_list[cpu_min];
    met->values_count = cpu_max - cpu_min;
    met->values_size  = sizeof(uint64_t) * met->values_count;
    met->values_type  = ID_UINT64;
    met->values_units = "KHz";
    return NULL;
}

plug_message_receive(_act_cpu_freq_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_act_all_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_act_cpu_freq_set)
{
    char freq[16]; // Enough for a frequency in Hz
    act_t *act = (act_t *) data;
    uint64_t *freqs = (uint64_t *) act->values;
    uint32_t cpu;

    if (act->device >= tp->cpu_count || act->device < -1) {
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
        sprintf(freq, "%lu", freqs[0]);
        filemagic_word_mwrite(fds_sss, tp->cpu_count, freq, 1);
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
        sprintf(freq, "%lu", freqs[cpu]);
        cpu_freq_governor_set(cpu, Governor.userspace);
        filemagic_word_write(fds_sss[cpu], freq, strlen(freq), 1);
    }
    return NULL;
}

plug_message_receive(_act_cpu_freq_reset)
{
    if (strlen(default_frequency) > 0 && strstr(default_frequency, "unsup") != NULL) {
        filemagic_word_mwrite(fds_sss, tp->cpu_count, default_frequency, 1);
    }
    cpu_freq_governor_reset(-1);
    return NULL;
}