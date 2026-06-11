#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <common/types.h>
#include <plugins/general/query_handler.h>
#include <plugins/general/metrics_central.h>

#define MAX_METRICS 4

typedef struct _act_s {
    int32_t   device;
    char     *sub_device;
    uint64_t *values;
    uint32_t  values_count;
    uint32_t  allocated_count; // Private use only
} _act_t;

static met_t   *metrics; // cpu_freqX2 and cpu_powX2
static void   **metrics_pointers; // Used when asking for 'all' metrics
static _act_t  *actuators; // Only one actuator per request is allowed

plug_identity()
{
    *tag       = "metrics_central";
    *tags_deps = "!topology_ext";
}

plug_action_init(_metrics_central)
{
    int i;
    metrics          = calloc(MAX_METRICS, sizeof(met_t));
    metrics_pointers = calloc(MAX_METRICS, sizeof(void *));
    actuators        = calloc(1, sizeof(_act_t));
    for (i = 0; i < 4; ++i) metrics_pointers[i] = &metrics[i];
    for (i = 0; i < 1; ++i) actuators[i].allocated_count = 1024;
    for (i = 0; i < 1; ++i) actuators[i].values = calloc(1024, sizeof(uint64_t));
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

static void expand_actuators(_act_t *act)
{
    act->allocated_count *= 2;
    act->values = realloc(act->values, act->allocated_count * sizeof(uint64_t));
}

static void parse_actuators(char *string, _act_t *act)
{
    char *start = NULL;
    char *end = NULL;

    act->values_count = 0;
    memset(act->values, 0, sizeof(uint64_t) * (act->allocated_count));
    // Parsing all the words
    while (*string != '\0') {
        // Cleaning spaces
        while (*string != '\0' && isspace((unsigned char)*string)) {
            string++;
        }
        // Found a word but seems to be the last
        if (*string == '\0') {
            return;
        }
        // Found word
        start = string;
        // Until the end of the word
        while (*string != '\0' && !isspace((unsigned char)*string)) {
            string++;
        }
        if (*string == '\0') {
            act->values[act->values_count++] = (uint64_t) atoll(start);
            return;
        }
        end = string, *end = '\0';
        act->values[act->values_count++] = (uint64_t) atoll(start);
        if (act->values_count == act->allocated_count) {
            expand_actuators(act);
        }
        *end = ' ';
        ++string;
    }
}

static char *form_message(query_t *query)
{
    static char msg[128] = {0};
    sprintf(msg, "%s_%s_%s", query->domain, query->metric, query->command);
    return msg;
}

void print_metrics(met_t *metric)
{
    int i;

    while (metric->values != NULL) {
        printf("%s values (in %s):\n", metric->metric_name, metric->values_units);
        for (i = 0; i < metric->values_count; ++i) {
            if (metric->values_type == ID_UINT32) printf("%u ", ((uint32_t *) metric->values)[i]);
            if (metric->values_type == ID_UINT64) printf("%lu ", ((uint64_t *) metric->values)[i]);
            if (metric->values_type == ID_DOUBLE) printf("%0.2lf ", ((double *) metric->values)[i]);
        }
        printf("\n");
        ++metric;
    }
}

static char *command_set(query_t *query, char *message)
{
    parse_actuators(query->arguments, &actuators[0]);

    if (actuators[0].values_count == 0) {
        return "No actuator values specified";
    }
    if (strncasecmp(query->device, "all", 3) != 0) {
        actuators[0].device = atoi(query->device);
    } else {
        actuators[0].device = -1;
    }
    actuators[0].sub_device = query->sub_device;
    plug_message_send(message, (void *) &actuators[0]);
    return NULL;
}

static void serialize_answer(met_t *metrics, answer_t *ans)
{
    // TODO
}

static char *command_get(query_t *query, char *message)
{
    int dev = 0;

    memset(metrics, 0, sizeof(met_t)*MAX_METRICS);
    // 'all' by default
    for (dev = 0; dev < MAX_METRICS; ++dev) {
        metrics[dev].device = -1;
        metrics[dev].sub_device = query->sub_device;
    }
    // An 'all' for metrics is also 'all' for devices
    if (strncasecmp(query->metric, "all", 3) == 0) {
        plug_message_send_each(message, metrics_pointers);
    } else {
        // If not 'all'
        if (strncasecmp(query->device, "all", 3) != 0) {
            metrics[0].device = atoi(query->device);
        }
        plug_message_send(message, (void *) &metrics[0]);
    }
    serialize_answer(metrics, &query->answer);
    print_metrics(metrics);
    return NULL;
}

plug_message_receive(_query_metrics)
{
    query_t *query = (query_t *) data;
    // Getting the function name of the plugin that will be called
    char *message = form_message(query);
    // TODO: check if the query is valid
    if (query->command[0] == 's') {
        return command_set(query, message);
    } else if (query->command[0] == 'g') {
        return command_get(query, message);
    }
    return NULL; // Or return invalid
}