#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <common/framework.h>
#include <common/utils/keeper.h>
#include <common/metrics_types.h>
#include <common/string_enhanced.h>

static metric_t *outputs;

plug_identity()
{
    *tag       = "metrics_caller";
    *tags_deps = NULL;
}

plug_init(_metrics_caller)
{
    outputs = calloc(MAX_METRICS_NO, sizeof(metric_t));
    // Keeper is a text file manager used to save default system values.
    keeper_open("/tmp/seanergys-defaults.conf");
    return NULL;
}

static char *form_message(metric_t *input)
{
    static char msg[128] = {0};
    sprintf(msg, "%s_%s_%s_%s", input->domain_label, input->metric_label,
        input->device_label, input->command_label);
    strtolow(msg);
    return msg;
}

static char *call_metrics_plugins(void *data)
{
    metrics_request_t *req = (metrics_request_t *) data;
    char *message = form_message(&req->input);
    // Preparing returning output
    memset(outputs, 0, sizeof(metric_t) * MAX_METRICS_NO);
    req->outputs = outputs;
    req->outputs_count = 0;
    //printf("%s\n", message);
    plug_message_send(message, data);
    return NULL;
}

plug_message_receive(_metrics_call)
{
    return call_metrics_plugins(data);
}

plug_message_receive(_metrics_call_indirect)
{
    // Used when some other plugin definition of metrics_call() overlaps.
    return call_metrics_plugins(data);
}