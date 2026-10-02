#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <common/hardware/bithack.h>
#include <metrics/common/hsmp.h>

static topology_t     *tp;
static metric_t        act;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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
    // Initializations
    act.domain      = Actuator;
    act.domain_name = MET_NAME(Actuator);
    act.device      = Cpu;
    act.device_name = MET_NAME(Cpu);
    act.devs_count  = tp->cpu_count;
    act.metric      = Freq;
    act.metric_name = MET_NAME(Freq);
    act.values      = calloc(act.devs_count, sizeof(uint64_t));
    act.units       = "KHz";
    act.granul      = act.device;
    act.granul_name = act.device_name;
    return NULL;
}

static void act_copy(metric_t *out, metric_t *act, uint64_t *copy, int cpu_min, int cpu_max)
{
    if (cpu_max == 0) return;
    memcpy(out, act, sizeof(metric_t)), out->values = NULL; // Meta-data only
    out->values_count = cpu_max - cpu_min; // #Values/devices
    if ((out->values = copy) == NULL) return; // The caller does not want values
    memcpy(copy, &act->values[cpu_min], sizeof(uint64_t)*out->values_count);
}

static char *acts_read(void *data, uint64_t *copy)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *met = &req->input;
    uint args[2]  = {0, -1};
    uint reps[2]  = {0, -1};
    int o         = req->outputs_count;
    int cpu_max   = tp->cpu_count;
    int cpu_min   = 0;
    int cpu       = 0;

    if (met->device_no != DEVS_ALL) {
        cpu_max = met->device_no + 1;
        cpu_min = met->device_no;
    }
    // Reading the values
    while (pthread_mutex_trylock(&lock));
    for (cpu = cpu_min; cpu < cpu_max; ++cpu) {
        act.values[cpu] = 0LLU;
        // Preparing HSMP arguments and response
        args[0] = tp->cpus[cpu].apicid;
        reps[0] = 0;
        // Calling HSMP, response is freq (MHz)
        if (state_ok(hsmp_send(tp->cpus[cpu].socket_id, HSMP_GET_BOOST_LIMIT, args, reps))) {
            act.values[cpu] = (uint64_t) reps[0];
        }
    }
    // Copying metric
    act_copy(&req->outputs[o], &act, &copy[cpu_min], cpu_min, cpu_max);
    pthread_mutex_unlock(&lock);
    req->outputs_count++;
    return NULL;
}

static char *acts_return(void *data)
{
    static __thread uint64_t *copy = NULL;
    // Each thread here will have its own copy of values.
    if (copy == NULL) {
        copy = calloc(act.devs_count, sizeof(uint64_t));
    }
    return acts_read(data, copy);
}

plug_message_receive(_act_freq_cpu_get)
{
    return acts_return(data);
}

plug_message_receive(_act_all_all_get)
{
    return acts_return(data);
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

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *out = &req->outputs[req->outputs_count];
    act_copy(out, &act, NULL, 0, act.devs_count);
    req->outputs_count++;
    return NULL;
}