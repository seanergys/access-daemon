#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <plugins/general/data_plane.h>
#include <plugins/general/request_handler.h>

static char     *output_buffer;
static uint64_t *arguments_values;

plug_identity()
{
    *tag       = "request_handler_text";
    *tags_deps = NULL;
}

plug_init(_request_handler_text)
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

static int is(char *s1, const char *s2)
{
    return strncasecmp(s1, s2, strlen(s1)) == 0;
}

static char *process_input_metrics_toint(metrics_input_t *input, char *arguments)
{
    input->device_no = (input->device_no_name[0] == 'a')? -1 : atoi(input->device_no_name);
    if      (is(input->domain_name , MET_NAME(Met)    )) { input->domain  = Met;        }
    else if (is(input->domain_name , MET_NAME(Act)    )) { input->domain  = Act;        }
    if      (is(input->command_name, MET_NAME(Get)    )) { input->command = Get;        }
    else if (is(input->command_name, MET_NAME(Set)    )) { input->command = Set;        }
    else if (is(input->command_name, MET_NAME(Info)   )) { input->command = Info;       }
    if      (is(input->device_name , MET_NAME(All)    )) { input->device  = All; }
    else if (is(input->device_name , MET_NAME(Cpu)    )) { input->device  = Cpu;        }
    else if (is(input->device_name , MET_NAME(Socket) )) { input->device  = Socket;     }
    else if (is(input->device_name , MET_NAME(Uncore) )) { input->device  = Uncore;     }
    else if (is(input->device_name , MET_NAME(Gpu)    )) { input->device  = Gpu;        }
    if      (is(input->metric_name , MET_NAME(All)    )) { input->metric  = All;        }
    else if (is(input->metric_name , MET_NAME(Pow)    )) { input->metric  = Pow;        }
    else if (is(input->metric_name , MET_NAME(Pwr)    )) { input->metric  = Pwr;        }
    else if (is(input->metric_name , MET_NAME(Energy) )) { input->metric  = Energy;     }
    else if (is(input->metric_name , MET_NAME(Freq)   )) { input->metric  = Freq;       }
    else if (is(input->metric_name , MET_NAME(AccFreq))) { input->metric  = AccFreq;    }
    else if (is(input->metric_name , MET_NAME(FreqMem))) { input->metric  = FreqMem;    }
    else if (is(input->metric_name , MET_NAME(AccFreqMem))) { input->metric = AccFreqMem; }
    // TODO: ¿Arguments?
}

static char *process_output_metrics_tochar(metrics_call_t *parsed)
{
    int o, v, c;

    memset(output_buffer, 0, sizeof(output_buffer));
    // Error checking
    if (parsed->state == Error) {
        sprintf(output_buffer, "Error: %s\n", parsed->error_msg);
        return output_buffer;
    }
    for (o = c = 0; o < parsed->outputs_count; ++o) {
        c += sprintf(&output_buffer[c], "%s_%s (%s)",
            parsed->outputs[o].device_name,
            parsed->outputs[o].metric_name,
            parsed->outputs[o].units);
        c += sprintf(&output_buffer[c], ": ");
        if (parsed->input.command == Info) {
            c += sprintf(&output_buffer[c], "%d devices", parsed->outputs[o].devs_count);
        }
        if (parsed->input.command == Get) {
            for (v = 0; v < parsed->outputs[o].values_count; ++v) {
                c += sprintf(&output_buffer[c], "%lu ", parsed->outputs[o].values[v]);
            }
        }
        c += sprintf(&output_buffer[c], "\n");
    }
    return output_buffer;
}

static char *process_text_metrics(request_text_t *req)
{
    metrics_call_t parsed = {0};
    const char *string = req->input;

    // Parsing
    if (!(string = wrdcpy(string, parsed.input.domain_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.metric_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_name   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_no_name))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.command_name  ))) return NULL;
    process_input_metrics_toint(&parsed.input, (char *) string);
    plug_message_send("metrics_call", (void *) &parsed);
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

plug_message_receive(_received_request_text)
{
    return process_text(data);
}