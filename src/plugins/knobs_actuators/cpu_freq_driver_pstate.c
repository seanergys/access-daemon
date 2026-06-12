#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>

static topology_t *tp;
static uint64_t   *current_list;

plug_identity()
{
    *tag       = "act_cpu_freq_driver_pstate";
    *tags_deps = "!topology_ext,!act_cpu_freq_driver_governor";
}

#define PATH_SMF  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq" // line break
#define PATH_SGV  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor" // line break

static int *fds_smf; // FD scaling max freq
static int *fds_sgv;

plug_action_init(_topology_ext)
{
    tp = (topology_t *) data;
    // Testing if frequency/governor set files are writable
    int set_frequency = filemagic_can_mwrite(PATH_SMF, tp->cpu_count, &fds_smf);
    int set_governor  = filemagic_can_mwrite(PATH_SGV, tp->cpu_count, &fds_sgv);
    if (!set_frequency) return rsprintf("[X] frequencies can not be set: %s", state_msg);
    if (!set_governor ) return rsprintf("[X] governors can not be set: %s", state_msg);
    // intel_pstate driver does not have governor "userspace".
    current_list = calloc(tp->cpu_count, sizeof(uint64_t));
    // TODO: save the default frequency and governor in a file or something
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
        if (filemagic_word_read(fds_smf[cpu], value, 1)) {
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
        sprintf(freq, "%lu", freqs[0]);
        filemagic_word_mwrite(fds_smf, tp->cpu_count, freq, 1);
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
        filemagic_word_write(fds_smf[cpu], freq, strlen(freq), 1);
    }
    return NULL;
}

plug_message_receive(_act_cpu_freq_reset)
{
    // TODO: recover the default governor and frequency from file
    return NULL;
}