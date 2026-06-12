#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/framework.h>
#include <plugins/general/metrics_central.h>
#include <management/cpupow/archs/intel63.c>

static topology_t       *tp;
static mgt_cpupow_ops_t  ops;
static uint64_t         *current_knobs;
static uint32_t         *temporal_values; // EAR uses integers

plug_identity()
{
    *tag       = "act_cpu_pow_msr";
    *tags_deps = "!topology_ext";
}

plug_action_init(_topology_ext)
{
    tp = (topology_t *) data; // And we save the topology for our own use
    // This is a big class, so I'm invoking the EAR code directly.
    mgt_cpupow_intel63_load(tp, &ops);
    if (ops.powercap_set == NULL) {
        return "[X] not enough privileges to read MSR";
    }
    // At least, the socket powercap should be supported.
    if (!ops.powercap_is_capable(CPUPOW_SOCKET)) {
        return "[X] powercap is not supported";
    }
    current_knobs = calloc(tp->cpu_count, sizeof(uint64_t));
    temporal_values = calloc(tp->cpu_count, sizeof(uint32_t));
    return NULL;
}

static int parse_subdevice(char *sub_device, int *devs_count)
{
    // Default number of devices
    if (devs_count != NULL) *devs_count = tp->socket_count;
    if (strncasecmp(sub_device, "socket", 6) == 0) return CPUPOW_SOCKET;
    if (strncasecmp(sub_device, "mem"   , 3) == 0) return CPUPOW_DRAM;
    if (strncasecmp(sub_device, "dram"  , 4) == 0) return CPUPOW_DRAM;
    if (strncasecmp(sub_device, "pp1"   , 3) == 0) return CPUPOW_UNCORE;
    if (strncasecmp(sub_device, "uncore", 6) == 0) return CPUPOW_UNCORE;
    if (strncasecmp(sub_device, "cpu"   , 3) == 0 ||
        strncasecmp(sub_device, "pp0"   , 3) == 0 ||
        strncasecmp(sub_device, "core"  , 4) == 0) {
        if (devs_count != NULL) *devs_count = tp->core_count;
        return CPUPOW_CORE;
    }
    return CPUPOW_SOCKET; // Socket by default
}

static char *return_actuators_values(void *data)
{
    met_t *met     = (met_t *) data;
    int cpu_max    = 0;
    int cpu_min    = 0;
    int cpu        = 0;
    int sub_device = parse_subdevice(met->sub_device, &cpu_max);

    memset(temporal_values, 0, sizeof(uint)*tp->cpu_count);
    if (met->device != -1) {
        cpu_max = met->device + 1;
        cpu_min = met->device;
    }
    ops.powercap_get(sub_device, (uint *) temporal_values);
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        current_knobs[cpu] = (uint64_t) temporal_values[cpu];
    }
    // Returning metric
    met->metric_name  = "cpu_pow";
    met->values       = &current_knobs[cpu_min];
    met->values_count = cpu_max - cpu_min;
    met->values_size  = sizeof(uint64_t) * met->values_count;
    met->values_type  = ID_UINT64;
    met->values_units = "W";
    return NULL;
}

plug_message_receive(_act_cpu_pow_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_act_all_get)
{
    return return_actuators_values(data);
}

plug_message_receive(_act_cpu_pow_set)
{
    act_t *act     = (act_t *) data;
    uint64_t *received_values = (uint64_t *) act->values;
    int cpu        = 0;
    int cpu_max    = 0;
    int sub_device = parse_subdevice(act->sub_device, &cpu_max);

    if (act->device >= tp->cpu_count || act->device < -1) {
        return "[X] Invalid device";
    }
    // Fixing values count
    if (act->values_count > cpu_max) {
        act->values_count = cpu_max;
    }
    // Setting default values
    for (cpu = 0; cpu < cpu_max; ++cpu) {
        temporal_values[cpu] = POWERCAP_DO_NOTHING;
    }
    // If received 'all devices' but just one actuator value, it means all the
    // actuators should be set to the same value.
    if (act->device == -1 && act->values_count == 1) {
        for (cpu = 0; cpu < cpu_max; ++cpu) {
            temporal_values[cpu] = (uint32_t) received_values[0];
        }
        ops.powercap_set(sub_device, temporal_values);
        return NULL;
    }
    // Preparing the per-device iteration
    if (act->device == -1) {
        act->device = 0;
    } else {
        act->values_count = act->device + 1;
    }
    // If received 'all devices', but multiple values, we write them in order
    for (cpu = act->device; cpu < act->values_count; ++cpu) {
        temporal_values[cpu] = (uint32_t) received_values[cpu];
    }
    ops.powercap_set(sub_device, (uint *) temporal_values);
    return NULL;
}

plug_message_receive(_act_cpu_pow_reset)
{
    act_t *act = (act_t *) data;
    ops.powercap_reset(parse_subdevice(act->sub_device, NULL));
    return NULL;
}