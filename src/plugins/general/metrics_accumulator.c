#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <plugins/general/metrics_accumulator.h>

static pthread_mutex_t   lock = PTHREAD_MUTEX_INITIALIZER;
static metrics_request_t acc; // General accumulation of metrics

plug_identity()
{
    *tag       = "metrics_accumulator";
    *tags_deps = NULL;
}

plug_init(_metrics_accumulator)
{
    static uint32_t enabled = 1;
    // Sharing some information with other plugins that are implementing the
    // metrics_accumulator initialization function.
    *data_alloc = &enabled;
    return NULL;
}

plug_init_post(_metrics_accumulator)
{
    sprintf(acc.input.domain_label , "met");
    sprintf(acc.input.metric_label , "all");
    sprintf(acc.input.device_label , "all");
    sprintf(acc.input.command_label, "get");
    acc.input.domain    = Met;
    acc.input.device    = All;
    acc.input.metric    = All;
    acc.input.device_no = All;
    acc.input.command   = GetAcc;
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

static void get_storage(int o)
{
    // TODO: use TLS to return a thread-safe copy of the values
}

plug_message_receive(_metrics_call)
{
    static __thread metric_t *outs = NULL; // Returning data
    metrics_request_t *req = (metrics_request_t *) data;
    int dev_max, dev_min;
    int a, o;

    // If a new thread enters here it requires its own returning outputs.
    if (outs == NULL) {
        outs = calloc(MAX_METRICS_NO, sizeof(metric_t));
    } else {
        memset(outs, 0, sizeof(metric_t) * MAX_METRICS_NO);
    }
    req->outputs = outs;
    // If it is not a Metric nor Get, let the call pipeline to continue
    if (req->input.domain  != Metric) { return NULL; }
    if (req->input.command != Get   ) { return NULL; }
    //printf("%s_%s_%s_%s\n", req->input.domain_label, req->input.metric_label,
    //    req->input.device_label, req->input.command_label);
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
        memcpy(&outs[o], &acc.outputs[a], sizeof(metric_t));
        // Getting a pointer to metrics or derived
        outs[o].values = &acc.outputs[a].values[dev_min];
        outs[o].values_count = dev_max - dev_min;
        // Increasing one
        req->outputs_count = ++o;
        // Cutting the comms now because is a specific metric
        if (req->input.device != AllDevices &&
            req->input.metric != AllMetrics) break;
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