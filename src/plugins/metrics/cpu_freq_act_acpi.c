#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>

#define MSR_IA32_APERF 0x000000E8
#define MSR_IA32_MPERF 0x000000E7

typedef struct sensor_s {
    uint64_t aperf;
    uint64_t mperf;
} sensor_t;

static topology_t  tp;
static apinfo_t    info;

plug_identity()
{
    *tag       = "cpu_freq_act_intel";
    *tags_deps = "!topology_ext";
}

plug_action_init(_topology)
{
    return NULL;
}

plug_message_receive(get)
{
    // Diff from a value
}

plug_message_receive(get_all)
{
    // Diff from a array
}