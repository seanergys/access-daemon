#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/hardware/bithack.h>
#include <metrics/common/hsmp.c>
#include <common/metrics_types.h>

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
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t   *met = &req->input;
    uint args[2] = {0, -1};
    uint reps[2] = {0, -1};
    int o        = req->outputs_count;
    int cpu_max  = tp->cpu_count;
    int cpu_min  = 0;
    int cpu      = 0;

    if (met->device_no != DEVS_ALL) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
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
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t   *act = &req->input;
    uint args[2] = { 0, -1 };
    uint reps[1] = { -1 };
    int cpu;

    if (act->device_no >= tp->cpu_count ||
        act->device_no <  DEVS_ALL) {
        return "[X] Invalid device";
    }
    // Fixing values count
    if (act->values_count > tp->cpu_count) {
        act->values_count = tp->cpu_count;
    }
    // If received 'all devices' but just one actuator value, it means all the
    // actuators should be set to the same value.
    if (act->device == DEVS_ALL && act->values_count == 1) {
        for (cpu = 0; cpu < tp->cpu_count; ++cpu) {
            // TODO: validate if the frequency is in the range
            args[0]  = setbits32(0, tp->cpus[cpu].apicid, 31, 16);
            args[0] |= ((((uint) act->values[0]) / 1000) & 0xFFFF);
            // TODO: discriminate turbo values from (sub)-nominals
            hsmp_send(tp->cpus[cpu].socket_id, HSMP_SET_BOOST_LIMIT, args, reps);
            // TODO: error control
        }
        return NULL;
    }
    // Here, values_count is equal or less than cpu_count
    if (act->device_no != DEVS_ALL) {
        act->values_count = act->device + 1;
    } else {
        act->device_no = 0;
    }
    // If all devices, but multiple values, we write them in order
    for (cpu = act->device_no; cpu < act->values_count; ++cpu) {
        // TODO: validate if the frequency is in the range
        args[0]  = setbits32(0, tp->cpus[cpu].apicid, 31, 16);
        args[0] |= ((((uint) act->values[cpu]) / 1000) & 0xFFFF);
        // TODO: discriminate turbo values from (sub)-nominals
        hsmp_send(tp->cpus[cpu].socket_id, HSMP_SET_BOOST_LIMIT, args, reps);
        // TODO: error control
    }
    return NULL;
}

plug_message_receive(_act_freq_cpu_reset)
{
    // TODO:
    return NULL;
}