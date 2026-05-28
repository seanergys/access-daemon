#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>

#define PATH_SSS  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed"
#define PATH_SGV  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"
#define PATH_SAG0 "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors"

static topology_t *tp;
static int *fds_sgv;
static int *fds_sss;

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

    if (!set_frequency || !set_governor) {
        return "[X]";
    }
    if (!is_userspace_available()) {
        return "[X]";
    }
    return NULL;
}

plug_message_receive(_cpu_freq_act_set)
{
    met_t *met = (met_t *) data;
    char freq[12]; // Enough for a frequency in Hz
    int cpu;

    if (met->device == -1) {
        // set governor
        filemagic_word_mwrite(fds_sss, tp->cpu_count, freq, 1);
    } else {
        for (cpu = 0; cpu < tp->cpu_count; ++cpu) {
            // set governor
            filemagic_word_write(fds_sss[cpu], freq, strlen(data), 0);
        }
    }
}