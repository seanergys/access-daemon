#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <plugins/general/metrics_central.h>

static topology_t *tp;
static uint64_t    base_frequency;

plug_identity()
{
    *tag       = "cpu_freq_met_dummy"; // This is my name. It's required because
                                       // someone could change the name of the
                                       // file.
    *tags_deps = "!topology_ext"; // The required plugins in comma sepparated
                                  // list. The '!' tells the framework to exit
                                  // because this plugin can't work.
}

plug_action_init(_cpu_freq_met_dummy)
{
    // This function is called sometime in the initialization process.
    return "Hello, I'm plug_action_init(_cpu_freq_met_dummy)";
}

plug_action_init(_topology_ext)
{
    // This function is called after the initialization of topology_ext plugin
    // and comes with an initialized topology in form of void pointer (void *data)
    tp = (topology_t *) data; // And we save the topology for our own use
    return "Hello, I'm plug_action_init(_topology_ext)";
}

static char *return_metrics(void *data)
{
    static uint64_t khz = 1000000;
    met_t *met = (met_t *) data; // This struct is defined in metrics_central.h
    met->metric_name  = "cpu_freq";
    met->values       = &khz;
    met->values_count = 1;
    met->values_size  = sizeof(uint64_t) * met->values_count;
    met->values_type  = ID_UINT64;
    met->values_units = "KHz";
    return NULL;
}

plug_message_receive(_met_cpu_freq_get)
{
    // This function will be called when `metrics_central.so` receives a query
    // from the Data Plane (or faked), that asks for "cpu_freq" metrics.
    return return_metrics(data);
}

plug_message_receive(_all_met_get)
{
    // This function will be called when `metrics_central.so` receives a query
    // from the Data Plane (or faked), that asks for all the metrics and this
    // plugin has to provide its part.
    return return_metrics(data);
}