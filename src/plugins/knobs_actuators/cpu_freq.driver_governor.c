#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <common/utils/keeper.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>
#include <plugins/knobs_actuators/cpu_freq.driver_governor.h>

// This is a helper plugin to control governor state within cpufreq drivers
#define PATH_SGV  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"
#define PATH_SAG0 "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors"

static topology_t *tp;
static int        *fds_sgv;
static char        default_governor[64];

plug_identity()
{
    *tag       = "act.cpu_freq.driver_governor";
    *tags_deps = "!topology_ext";
}

plug_init(_topology_ext)
{
    tp = (topology_t *) data;
    int set_governor = filemagic_can_mwrite(PATH_SGV, tp->cpu_count, &fds_sgv);
    if (!set_governor) return rsprintf("[X] governors can not be set: %s", state_msg);
    if (!keeper_load_text("CpuFreqGovernorDefault", default_governor)) {
        keeper_save_text("CpuFreqGovernorDefault", cpu_freq_governor_get(0, default_governor));
    }
    return NULL;
}

int cpu_freq_is_userspace_available()
{
    int found_userspace = 0;
    int fd_sag0         = -1;
    char buffer[32];

    if (!filemagic_can_read(PATH_SAG0, &fd_sag0)) return 0;
    // Read if there is userspace governor, if not this is not a valid driver.
    while (!found_userspace && filemagic_word_read(fd_sag0, buffer, 0)) {
        found_userspace = (strncasecmp(buffer, "userspace", 9) == 0);
    }
    close(fd_sag0);
    return found_userspace;
}

char *cpu_freq_governor_get(int cpu, char *buffer)
{
    if (!filemagic_word_read(fds_sgv[cpu], buffer, 1)) {
        buffer[0] = '\0';
    }
    return buffer;
}

int cpu_freq_governor_set(int cpu, char *governor)
{
    int ret;
    // If all CPUs
    if (cpu < 0) {
        ret = filemagic_word_mwrite(fds_sgv, tp->cpu_count, governor, 1);
    } else {
        ret = filemagic_word_write(fds_sgv[cpu], governor, strlen(governor), 1);
    }
    return ret;
}

int cpu_freq_governor_reset(int cpu)
{
    return cpu_freq_governor_set(cpu, default_governor);
}