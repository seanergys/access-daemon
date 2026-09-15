#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <plugins/general/metrics_accumulator.h>

static pthread_mutex_t   lock = PTHREAD_MUTEX_INITIALIZER;
static metrics_call_t acc; // General accumulation of metrics

plug_identity()
{
    *tag       = "metrics_accumulator";
    *tags_deps = NULL;
}

plug_init(_metrics_accumulator)
{
    sprintf(acc.input.domain_name , "met");
    sprintf(acc.input.metric_name , "all");
    sprintf(acc.input.device_name , "all");
    sprintf(acc.input.command_name, "get");
    acc.input.command   = GetAcc;
    acc.input.device    = AllDevices;
    acc.input.metric    = AllMetrics;
    acc.input.device_no = DEVS_ALL;
    return NULL;
}

plug_periodic(_metrics_accumulator)
{
    // Resetting
    acc.outputs_count = 0;
    // Every second (or periodic time set by the admin)
    plug_message_send("metrics_call_indirect", (void *) &acc);
    return NULL;
}

plug_message_receive(_metrics_call)
{
    static __thread metrics_output_t *outs = NULL; // Returning data
    metrics_call_t *req = (metrics_call_t *) data;
    int dev_max, dev_min;
    int a, o;

    // If a new thread enters here it requires its own returning outputs.
    if (outs == NULL) {
        outs = calloc(MAX_METRICS_NO, sizeof(metrics_output_t));
    } else {
        memset(outs, 0, sizeof(metrics_output_t) * MAX_METRICS_NO);
    }
    req->outputs = outs;
    // If it is an actuator, do nothing
    if (strcasecmp(req->input.domain_name, MET_NAME(Actuator)) == 0) {
        return NULL;
    }
    // If is a metric then this is a trap, do not call any metrics plugin
    while (pthread_mutex_trylock(&lock));
    for (a = o = 0; a < acc.outputs_count; a++) {
        if (req->input.device != AllDevices && req->input.device != acc.outputs[a].device) {
            continue;
        }
        if (req->input.metric != AllMetrics && req->input.metric != acc.outputs[a].metric) {
            continue;
        }
        // Getting the minimum device number
        if (req->input.device_no != DEVS_ALL) {
            dev_min = req->input.device_no;
            dev_max = dev_min + 1;
        } else {
            dev_max = acc.outputs[a].values_count;
            dev_min = 0;
        }
        // Copying the metric information
        memcpy(&outs[o], &acc.outputs[a], sizeof(metrics_output_t));
        // Getting a pointer to metrics or derived
        outs[o].values = &acc.outputs[a].values[dev_min];
        outs[o].values_count = dev_max - dev_min;
        // Increasing one
        req->outputs_count = ++o;
        // Cutting the comms now because is a specific metric
        if (req->input.metric != AllMetrics) break;
        if (req->input.device != AllDevices) break;
    }
leave:
    pthread_mutex_unlock(&lock);
    if (req->outputs_count == 0) {
        req->state     = Error;
        req->error_msg = "Metric not found.";
    }
    // TODO: do not print the [S]
    return "[S]";
}