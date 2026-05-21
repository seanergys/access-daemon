#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <common/types.h>
#include <plugins/general/query_handler.h>
#include <plugins/general/metrics_central.h>

static met_t metrics[4]; // cpu_freqX2 and cpu_powX2

plug_identity()
{
    *tag       = "metrics_central";
    *tags_deps = "!topology_ext";
}

plug_action_init(_metrics_central)
{
    return NULL;
}

plug_action_periodic(_metrics_central)
{
    return NULL;
}

static int is(char *s1, const char *s2)
{
    return strcasecmp(s1, s2) == 0;
}

static char *form_message(query_t *query)
{
    static char msg[128] = {0};
    sprintf(msg, "%s_%s_%s", query->metric, query->domain, query->command);
    return msg;
}

void print_metrics(met_t *metric)
{
    int i;
    printf("%s values (in %s):\n", metric->metric_name, metric->values_units);
    for (i = 0; i < metric->values_count; ++i) {
        if (metric->values_type == ID_UINT64) printf("%lu ", ((uint64_t *) metric->values)[i]);
        if (metric->values_type == ID_DOUBLE) printf("%0.2lf ", ((double *) metric->values)[i]);
    }
    printf("\n");
}

plug_message_receive(_query_metrics)
{
    query_t *query = (query_t *) data;
    char *message = form_message(query);
    // all is a special wildcard
    metrics[0].device = strncmp(query->device, "all", 3) == 0 ? -1 : atoi(query->device);
    plug_message_send(message, (void *) &metrics[0]);
    query->answer = &metrics[0].values;
    query->answer_size = metrics[0].values_size;
    print_metrics(&metrics[0]);
    return NULL;
}