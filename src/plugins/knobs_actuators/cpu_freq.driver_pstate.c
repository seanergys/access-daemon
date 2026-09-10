#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <common/utils/keeper.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>
#include <plugins/knobs_actuators/cpu_freq.driver_governor.h>

static topology_t *tp;
static uint64_t   *current_list;
static char        default_frequency[64];

plug_identity()
{
    *tag       = "act.cpu_freq.driver_pstate";
    *tags_deps = "!topology_ext,!act.cpu_freq.driver_governor";
}

#define PATH_SMF  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq" // line break

static int *fds_smf; // FD scaling max freq
static int *fds_sgv;

plug_init(_topology_ext)
{
    tp = (topology_t *) data;
    // Testing if frequency/governor set files are writable
    int set_frequency = filemagic_can_mwrite(PATH_SMF, tp->cpu_count, &fds_smf);
    if (!set_frequency) return rsprintf("[X] frequencies can not be set: %s", state_msg);
    // intel_pstate driver does not have governor "userspace".
    current_list = calloc(tp->cpu_count, sizeof(uint64_t));
    // Saving default frequency
    if (!keeper_load_text("CpuFreqDefault", default_frequency)) {
        if (filemagic_word_read(fds_smf[0], default_frequency, 1)) {
            keeper_save_text("CpuFreqDefault", default_frequency);
        }
    }
    return NULL;
}

static char *return_actuators_values(void *data)
{
    request_metrics_t *req = (request_metrics_t *) data;
    metrics_input_t   *met = &req->input;
    int o          = req->outputs_count;
    char value[64] = {0};
    int cpu_max    = tp->cpu_count;
    int cpu_min    = 0;
    int cpu        = 0;

    if (met->device_no != -1) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
    }
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        current_list[cpu] = 0LLU;
        if (filemagic_word_read(fds_smf[cpu], value, 1)) {
            current_list[cpu] = (uint64_t) atoll(value);
        }
    }
    // Returning metric
    req->outputs[o].device       = Cpu;
    req->outputs[o].device_name  = MET_NAME(Cpu);
    req->outputs[o].metric       = Freq;
    req->outputs[o].metric_name  = MET_NAME(Freq);
    req->outputs[o].values       = &current_list[cpu_min];
    req->outputs[o].units        = "KHz";
    req->outputs[o].values_count = cpu_max - cpu_min;
    req->outputs[o].granularity  = Cpu;
    req->outputs_count++;
    return NULL;
}

plug_message_receive(_act_freq_cpu_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_act_all_all_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_act_freq_cpu_set)
{
    char freq_str[16]; // Enough for a frequency in Hz
    request_metrics_t *req = (request_metrics_t *) data;
    metrics_input_t   *act = &req->input;
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
    if (act->device_no == DEVS_ALL && act->values_count == 1) {
        // TODO: validate if the frequency is in the range
        sprintf(freq_str, "%lu", act->values[0]);
        filemagic_word_mwrite(fds_smf, tp->cpu_count, freq_str, 1);
        return NULL;
    }
    // Here, values_count is equal or less than cpu_count
    if (act->device_no != DEVS_ALL) {
        act->values_count = act->device + 1;
    } else {
        act->device_no = 0;
    }
    // If all devices, but multiple values, we write them in order
    for (cpu = act->device; cpu < act->values_count; ++cpu) {
        // TODO: validate if the frequency is in the range
        sprintf(freq_str, "%lu", act->values[cpu]);
        filemagic_word_write(fds_smf[cpu], freq_str, strlen(freq_str), 1);
    }
    return NULL;
}

plug_message_receive(_act_freq_cpu_reset)
{
    if (strlen(default_frequency) > 0) {
        filemagic_word_mwrite(fds_smf, tp->cpu_count, default_frequency, 1);
    }
    cpu_freq_governor_reset(-1);
    return NULL;
}