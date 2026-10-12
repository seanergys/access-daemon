#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <plugins/general/metrics_accumulator.h>

static pthread_key_t     outputs_key;
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
    int             rc;

    // The returned buffer stays valid until the next request or thread exit.
    if ((rc = pthread_key_create(&outputs_key, free)) != 0) {
        return "[D] Failed to create metrics output key";
    }
    // Sharing some information with other plugins that are implementing the
    // metrics_accumulator initialization function.
    *data_alloc = &enabled;
    return NULL;
}

static char *accumulate()
{
    while (pthread_mutex_trylock(&lock));
    // Resetting
    acc.outputs_count = 0;
    // Every second (or periodic time set by the admin)
    plug_message_send("metrics_call_indirect", (void *) &acc);
    pthread_mutex_unlock(&lock);
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
    return accumulate();
}

plug_periodic(_metrics_accumulator)
{
    return accumulate();
}

plug_exit()
{
    free(acc.outputs);
    acc.outputs = NULL;
    acc.outputs_count = 0;
    return NULL;
}

plug_message_receive(_metrics_call)
{
    metrics_request_t *req = (metrics_request_t *) data;
    int dev_max, dev_min;
    metric_t *outs;
    int a, o;

    req->outputs_count = 0;
    req->outputs = NULL;
    // Thread local-storage (TLS) or thread specific-data (TSD)
    if ((outs = pthread_getspecific(outputs_key)) == NULL) {
        if ((outs = calloc(MAX_METRICS_NO, sizeof(metric_t))) == NULL) {
            return "[S] Failed to allocate metrics outputs";
        }
        if (pthread_setspecific(outputs_key, outs) != 0) {
            free(outs);
            return "[S] Failed to store metrics outputs";
        }
    }
    memset(outs, 0, sizeof(metric_t) * MAX_METRICS_NO);
    req->outputs = outs;
    // If it is not a Metric nor Get, let the call pipeline to continue
    if (req->input.domain  != Metric) { return NULL; }
    if (req->input.command != Get   ) { return NULL; }
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
    pthread_mutex_unlock(&lock);
    return "[S]";
}
