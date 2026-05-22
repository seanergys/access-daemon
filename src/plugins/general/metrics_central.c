#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <common/types.h>
#include <plugins/general/query_handler.h>
#include <plugins/general/metrics_central.h>

static met_t *metrics; // cpu_freqX2 and cpu_powX2
static void **metrics_pointers;

plug_identity()
{
    *tag       = "metrics_central";
    *tags_deps = "!topology_ext";
}

plug_action_init(_metrics_central)
{
    int i;
    metrics          = calloc(4, sizeof(met_t));
    metrics_pointers = calloc(4, sizeof(void *));
    for (i = 0; i < 4; ++i) metrics_pointers[i] = &metrics[i];
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

    while (metric->values != NULL) {
        printf("%s values (in %s):\n", metric->metric_name, metric->values_units);
        for (i = 0; i < metric->values_count; ++i) {
            if (metric->values_type == ID_UINT64) printf("%lu ", ((uint64_t *) metric->values)[i]);
            if (metric->values_type == ID_DOUBLE) printf("%0.2lf ", ((double *) metric->values)[i]);
        }
        printf("\n");
        ++metric;
    }
}

plug_message_receive(_query_metrics)
{
    query_t *query = (query_t *) data;
    char *message = form_message(query);

    memset(metrics, 0, sizeof(met_t)*4);
    // all is a special wildcard
    if (strncmp(query->metric, "all", 3) == 0) {
        metrics[0].device = -1;
        plug_message_send_each(message, metrics_pointers);
    } else {
        if (strncmp(query->device, "all", 3) != 0) {
            metrics[0].device = atoi(query->device);
        } else {
            metrics[0].device = -1;
        }
        plug_message_send(message, (void *) &metrics[0]);

    }
    print_metrics(metrics);
    query->answer = &metrics[0].values;
    query->answer_size = metrics[0].values_size;
    return NULL;
}