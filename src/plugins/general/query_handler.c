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

static char *process_text_configuration(request_text_t *req)
{
    char *string = req->input;
    char buffer[1024];
    char *p = NULL;

    while ((string = wrdcpy(string, buffer)) != NULL) {
        if ((p = strstr(string, "plugins=")) != NULL) {
            plugin_manager_load_more_plugins(&p[8]);
        }
    }
    return NULL;
}

static char *process_input_metrics_toint(metrics_input_t *input, char *arguments)
{
    input->device_no = (input->device_no_name[0] == 'a')? -1 : atoi(input->device_no_name);
    // Devices
    if      (strcasecmp(input->device_name, "all"            ) == 0) { input->device = AllMetrics; }
    else if (strcasecmp(input->device_name, MET_NAME(Cpu)    ) == 0) { input->device = Cpu;        }
    else if (strcasecmp(input->device_name, MET_NAME(Socket) ) == 0) { input->device = Socket;     }
    else if (strcasecmp(input->device_name, MET_NAME(Uncore) ) == 0) { input->device = Uncore;     }
    else if (strcasecmp(input->device_name, MET_NAME(Gpu)    ) == 0) { input->device = Gpu;        }
    // Metrics
    if      (strcasecmp(input->metric_name, "all"            ) == 0) { input->metric = AllDevices; }
    else if (strcasecmp(input->metric_name, MET_NAME(Pow)    ) == 0) { input->metric = Pow;     }
    else if (strcasecmp(input->metric_name, MET_NAME(Pwr)    ) == 0) { input->metric = Pwr;     }
    else if (strcasecmp(input->metric_name, MET_NAME(Energy) ) == 0) { input->metric = Energy;  }
    else if (strcasecmp(input->metric_name, MET_NAME(Freq)   ) == 0) { input->metric = Freq;    }
    else if (strcasecmp(input->metric_name, MET_NAME(AccFreq)) == 0) { input->metric = AccFreq; }
    // TODO: ¿Arguments?
}

static char *process_output_metrics_tochar(request_metrics_t *parsed)
{
    int o, v, c;

    memset(output_buffer, 0, sizeof(output_buffer));
    // Error checking
    if (parsed->state == Error) {
        sprintf(output_buffer, "Error: %s\n", parsed->error_msg);
        return output_buffer;
    }
    for (o = c = 0; o < parsed->outputs_count; ++o) {
        c += sprintf(&output_buffer[c], "%s_%s (%s): ",
            parsed->outputs[o].device_name,
            parsed->outputs[o].metric_name,
            parsed->outputs[o].units);
        for (v = 0; v < parsed->outputs[o].values_count; ++v) {
            c += sprintf(&output_buffer[c], "%lu ", parsed->outputs[o].values[v]);
        }
        c += sprintf(&output_buffer[c], "\n");
    }
    return output_buffer;
}

static char *process_text_metrics(request_text_t *req)
{
    request_metrics_t parsed = {0};
    const char *string = req->input;

    // Parsing
    if (!(string = wrdcpy(string, parsed.input.domain_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.metric_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_no_name))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.command_name  ))) return NULL;
    process_input_metrics_toint(&parsed.input, (char *) string);
    plug_message_send("request_metrics", (void *) &parsed);
    // Return the metrics converted into text
    req->output = process_output_metrics_tochar(&parsed);
    return NULL;
}

static char *process_text(void *data)
{
    request_text_t *req = (request_text_t *) data;
    const char *string = req->input;

    if (strncasecmp(string, "met", 3) == 0) return process_text_metrics(req);
    if (strncasecmp(string, "act", 3) == 0) return process_text_metrics(req);
    if (strncasecmp(string, "cfg", 3) == 0) return process_text_configuration(req);
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