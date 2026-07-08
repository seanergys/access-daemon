#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/hardware/bithack.h>
#include <metrics/common/hsmp.c>
#include <plugins/general/metrics_central.h>

static topology_t *tp;
static uint64_t   *current_list;

plug_identity()
{
    *tag       = "act.cpu_freq.hsmp";
    *tags_deps = "!topology_ext,!act_cpu_freq_driver_governor";
}

plug_init(_topology_ext)
{
    tp = (topology_t *) data; // And we save the topology for our own use
    if (state_fail(hsmp_open(tp, HSMP_WR))) {
        return rsprintf("[X] HSMP failed: %s", state_msg);
    }
    return NULL;
}

static char *return_actuators_values(void *data)
{
    uint args[2] = {0, -1};
    uint reps[2] = {0, -1};
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
        // Preparing HSMP arguments and response
        args[0] = tp->cpus[cpu].apicid;
        reps[0] = 0;
        // Calling HSMP, response is freq (MHz)
        if (state_ok(hsmp_send(tp->cpus[cpu].socket_id, HSMP_GET_BOOST_LIMIT, args, reps))) {
            current_list[cpu] = (uint64_t) reps[0];
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
    uint args[2] = { 0, -1};
    uint reps[1] = { -1 };
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
        for (cpu = 0; cpu < tp->cpu_count; ++cpu) {
            // TODO: validate if the frequency is in the range
            args[0]  = setbits32(0, tp->cpus[cpu].apicid, 31, 16);
            args[0] |= ((((uint) freqs[0]) / 1000) & 0xFFFF);
            // TODO: discriminate turbo values from (sub)-nominals
            hsmp_send(tp->cpus[cpu].socket_id, HSMP_SET_BOOST_LIMIT, args, reps);
            // TODO: error control
        }
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
        args[0]  = setbits32(0, tp->cpus[cpu].apicid, 31, 16);
        args[0] |= ((((uint) freqs[cpu]) / 1000) & 0xFFFF);
        // TODO: discriminate turbo values from (sub)-nominals
        hsmp_send(tp->cpus[cpu].socket_id, HSMP_SET_BOOST_LIMIT, args, reps);
        // TODO: error control
    }
    return NULL;
}

plug_message_receive(_act_cpu_freq_reset)
{
    // TODO:
    return NULL;
}