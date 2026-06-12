#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <metrics/common/file.h>
#include <plugins/general/metrics_central.h>

// This is a helper plugin to control governor state within cpufreq drivers
#define PATH_SGV  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"
#define PATH_SAG0 "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors"

static topology_t *tp;
static int        *fds_sgv;

plug_identity()
{
    *tag       = "act_cpu_freq_driver_governor";
    *tags_deps = "!topology_ext";
}

plug_action_init(_topology_ext)
{
    tp = (topology_t *) data;
    return NULL;
}

plug_message_receive(_act_cpu_freq_set_governor)
{
    return NULL;
}

plug_message_receive(_act_cpu_freq_reset_governor)
{
    return NULL;
}