#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>

#define PATH_SSS  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed"
#define PATH_SGV  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"
#define PATH_SAG0 "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors"

static topology_t *tp;
static int        *fds_sgv;
static int        *fds_sss;
static uint64_t   *current_list;

plug_identity()
{
    *tag       = "cpu_freq_act_acpi";
    *tags_deps = "!topology_ext";
}

static int is_userspace_available()
{
    int found_userspace = 0;
    int fd_sag0         = -1;
    char buffer[32];

    if (!filemagic_can_read(PATH_SAG0, &fd_sag0))
        return -1;
    // Read if there is userspace governor, if not this is not a valid driver.
    while (!found_userspace && filemagic_word_read(fd_sag0, buffer, 0)) {
        found_userspace = (strncasecmp(buffer, "userspace", 9) == 0);
    }
    close(fd_sag0);
    return found_userspace;
}

plug_action_init(_topology_ext)
{
    tp = (topology_t *) data;
    // Testing if frequency/governor set files are accessible
    int set_frequency = filemagic_can_mwrite(PATH_SSS, tp->cpu_count, &fds_sss);
    int set_governor  = filemagic_can_mwrite(PATH_SGV, tp->cpu_count, &fds_sgv);
    if (!set_frequency) return_msg("[X] Frequencies can not be set: %s", state_msg);
    if (!set_governor ) return_msg("[X] Governors can not be set: %s", state_msg);
    //if (!is_userspace_available()) return "[X] Userspace governor not available";
    current_list = calloc(tp->cpu_count, sizeof(uint64_t));
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

plug_message_receive(_cpu_freq_act_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_all_act_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_cpu_freq_act_set)
{
    char freq[12]; // Enough for a frequency in Hz
    met_t *met = (met_t *) data;
    uint64_t *freqs = (uint64_t *) met->values;
    uint cpu;

    // If all devices and just one actuator value, it means all the actuators
    // should be set to the same value.
    if (met->device == -1 && met->values_count == 1) {
        sprintf(freq, "%lu", freqs[0]);
        filemagic_word_mwrite(fds_sss, tp->cpu_count, freq, 1);
    }
    if (met->values > tp->cpu_count) {
        met->values_count = tp->cpu_count;
    }
    // If all devices, but multiple values, we write them in order
    for (cpu = 0; cpu < met->values_count; ++cpu) {
        sprintf(freq, "%lu", freqs[cpu]);
        filemagic_word_write(fds_sss[cpu], freq, strlen(freq), 0);
    }
    return NULL;
}