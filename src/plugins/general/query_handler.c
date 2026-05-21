#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plugins/general/query_handler.h>

plug_identity()
{
    *tag       = "query_handler";
    *tags_deps = NULL;
}

plug_action_init(_query_handler)
{
    // Configuration (plugins,periodicity)
    return NULL;
}

plug_action_periodic(_query_handler)
{
    return NULL;
}

int wrdcpy(const char *string, int i, char *dst)
{
    const char *start = NULL;
    const char *end = NULL;
    size_t length = 0;
    int j = 0;

    if (string == NULL || dst == NULL || i < 0) {
        return 0;
    }
    dst[0] = '\0';
    while (*string != '\0') {
        // Skipping initial spaces
        while (*string != '\0' && isspace((unsigned char)*string)) {
            string++;
        }
        if (*string == '\0') {
            break;
        }
        // Found word
        start = string;
        // Avanzar hasta el final de la palabra
        while (*string != '\0' && !isspace((unsigned char)*string)) {
            string++;
        }
        end = string;
        length = end - start;
        if (j == i) {
            memcpy(dst, start, length);
            dst[length] = '\0';
            return 1;
        }
        j++;
    }
    return 0;
}

static void answer(void *bytes, size_t size)
{
    // Do something
}

plug_message_receive(_keyboard)
{
    const char *string = (char *) data;
    query_t query = {0};
    // <metric> <domain> <command> <device> <param>
    // cpu_freq met get all/# (it performs a diff internally)
    // cpu_freq act get all/#
    // cpu_freq act set all/#
    // all      met get all   (special wildcard for Telemetry Collector)
    // aggregation not required, is done in other place
    wrdcpy(string, 0, query.metric);
    wrdcpy(string, 1, query.domain);
    wrdcpy(string, 2, query.command);
    wrdcpy(string, 3, query.device);
    query.device_toint = atoi(query.device);
    plug_message_send("query_metrics", &query);
    answer(query.answer, query.answer_size);
    return NULL;
}