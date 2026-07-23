#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plugins/general/data_plane.h>
#include <plugins/general/query_handler.h>

static char     *output_buffer;
static uint64_t *arguments_values;

plug_identity()
{
    *tag       = "query_handler";
    *tags_deps = NULL;
}

plug_init(_query_handler)
{
    output_buffer    = calloc(4096, 1);
    arguments_values = calloc(1024, sizeof(uint64_t));
    return NULL;
}

char *wrdcpy(const char *string, char *dst)
{
    const char *start = NULL;
    const char *end = NULL;
    size_t length = 0;

    if (string == NULL || dst == NULL) {
        return 0;
    }
    dst[0] = '\0';
    // Skipping initial spaces
    while (*string != '\0' && isspace((unsigned char)*string)) {
        string++;
    }
    if (*string == '\0') {
        return NULL;
    }
    // Found word
    start = string;
    // Avanzar hasta el final de la palabra
    while (*string != '\0' && !isspace((unsigned char)*string)) {
        string++;
    }
    end = string;
    length = end - start;
    memcpy(dst, start, length);
    dst[length] = '\0';
    return (char *) string;
}

static int isnum(const char *string)
{
    if (!string) {
        return 0;
    }
    while (isspace((unsigned char) *string)) {
        string++;
    }
    if (*string == '\0') {
        return 0;
    }
    while (*string && !isspace((unsigned char) *string)) {
        if (!isdigit((unsigned char) *string)) {
            return 0;
        }
        string++;
    }
    return 1;
}

static char *process_text_toint(metrics_input_t *input, char *arguments)
{
    input->device_no = (input->device_no_name[0] == 'a')? -1 : atoi(input->device_no_name);
    // Arguments
}

static char *process_text(void *data)
{
    static request_metrics_t parsed = {0};
    request_text_t *req = (request_text_t *) data;
    const char *string = req->input;
    int o, v, c;

    // Parsing
    if (!(string = wrdcpy(string, parsed.input.domain_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.metric_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_no_name))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.command_name  ))) return NULL;
    process_text_toint(&parsed.input, (char *) string);
    plug_message_send("request_metrics", (void *) &parsed);
    // Return the metrics converted into text
    for (o = c = 0; o < parsed.outputs_count; ++o) {
        c += sprintf(&output_buffer[c], "%s_%s (%s): ",
            parsed.outputs[o].device_name, parsed.outputs[o].metric_name, parsed.outputs[o].units);
        for (v = 0; v < parsed.outputs[o].values_count; ++v) {
            c += sprintf(&output_buffer[c], "%lu ", parsed.outputs[o].values[v]);
        }
        c += sprintf(&output_buffer[c], "\n");
    }
    req->output = output_buffer;
    return NULL;
}

plug_message_receive(_request_text)
{
    return process_text(data);
}

plug_message_receive(_request_metric)
{
    // More direct, no parsing needed
    return NULL;
}