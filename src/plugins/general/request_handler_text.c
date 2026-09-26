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

static char *process_text_configuration(metrics_request_text_t *req)
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

static char *process_output_metrics_tochar(metrics_request_t *parsed)
{
    int o, v, c;

    memset(output_buffer, 0, sizeof(output_buffer));
    // Error checking
    if (parsed->state == Error) {
        sprintf(output_buffer, "Error: %s\n", parsed->error_msg);
        return output_buffer;
    }
    for (o = c = 0; o < parsed->outputs_count; ++o) {
        c += sprintf(&output_buffer[c], "%s %s_%s (%s)",
            parsed->outputs[o].domain_name,
            parsed->outputs[o].device_name,
            parsed->outputs[o].metric_name,
            parsed->outputs[o].units);
        c += sprintf(&output_buffer[c], ": ");
        if (parsed->input.command == Info) {
            c += sprintf(&output_buffer[c], "%d devices ", parsed->outputs[o].devs_count);
            c += sprintf(&output_buffer[c], "(granularity %s)", parsed->outputs[o].granul_name);
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

static int is(char *s1, const char *s2)
{
    return strncasecmp(s1, s2, strlen(s2)) == 0;
}

static void process_input_metrics_toint(metric_t *input, char *arguments)
{
    char buffer[128];
    // TODO: use a temporal input to correct labels (i.e: Power or Pwr to Pow)
    if      (is(input->domain_label , MET_NAME(All)    )) { input->domain  = All;        }
    else if (is(input->domain_label , MET_NAME(Met)    )) { input->domain  = Met;        }
    else if (is(input->domain_label , MET_NAME(Act)    )) { input->domain  = Act;        }
    if      (is(input->device_label , MET_NAME(All)    )) { input->device  = All;        }
    else if (is(input->device_label , MET_NAME(Cpu)    )) { input->device  = Cpu;        }
    else if (is(input->device_label , MET_NAME(Thread) )) { input->device  = Thread;     }
    else if (is(input->device_label , MET_NAME(Core)   )) { input->device  = Core;       }
    else if (is(input->device_label , MET_NAME(Pp0)    )) { input->device  = Pp0;        }
    else if (is(input->device_label , MET_NAME(Socket) )) { input->device  = Socket;     }
    else if (is(input->device_label , MET_NAME(Pkg)    )) { input->device  = Pkg;        }
    else if (is(input->device_label , MET_NAME(Package))) { input->device  = Package;    }
    else if (is(input->device_label , MET_NAME(Uncore) )) { input->device  = Uncore;     }
    else if (is(input->device_label , MET_NAME(Pp1)    )) { input->device  = Pp1;        }
    else if (is(input->device_label , MET_NAME(Mem)    )) { input->device  = Mem;        }
    else if (is(input->device_label , MET_NAME(Dram)   )) { input->device  = Dram;       }
    else if (is(input->device_label , MET_NAME(Gpu)    )) { input->device  = Gpu;        }
    if      (is(input->metric_label , MET_NAME(All)    )) { input->metric  = All;        }
    else if (is(input->metric_label , MET_NAME(Pow)    )) { input->metric  = Pow;        }
    else if (is(input->metric_label , MET_NAME(Pwr)    )) { input->metric  = Pwr;        }
    else if (is(input->metric_label , MET_NAME(Power)  )) { input->metric  = Power;      }
    else if (is(input->metric_label , MET_NAME(Energy) )) { input->metric  = Energy;     }
    else if (is(input->metric_label , MET_NAME(Freq)   )) { input->metric  = Freq;       }
    else if (is(input->metric_label , MET_NAME(Frequency))) { input->metric = Frequency; }
    else if (is(input->metric_label , MET_NAME(AccFreq))) { input->metric  = AccFreq;    }
    else if (is(input->metric_label , MET_NAME(Bwidth) )) { input->metric  = Bwidth;     }
    else if (is(input->metric_label , MET_NAME(Bandwidth))) { input->metric = Bandwidth; }
    if      (is(input->command_label, MET_NAME(Get)    )) { input->command = Get;        }
    else if (is(input->command_label, MET_NAME(Set)    )) { input->command = Set;        }
    else if (is(input->command_label, MET_NAME(Info)   )) { input->command = Info;       }
    // Device number follow different rules
    if (is(input->device_no_label, MET_NAME(All))) { input->device_no = All; }
    else input->device_no = (uint64_t) atoi(input->device_no_label);
    // We assume text based request are coming from the keyboard plugin only.
    input->values_count = 0;
    while ((arguments = wrdcpy(arguments, buffer)) != NULL) {
        arguments_values[input->values_count] = (uint64_t) atoi(buffer);
        printf("arguments %lu\n", arguments_values[input->values_count]);
        input->values_count++;
    }
}

static char *process_text_metrics(metrics_request_text_t *req)
{
    metrics_request_t parsed = {0};
    const char *string = req->input;

    // Parsing labels
    if (!(string = wrdcpy(string, parsed.input.domain_label   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.metric_label   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_label   ))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.device_no_label))) return NULL;
    if (!(string = wrdcpy(string, parsed.input.command_label  ))) return NULL;
    process_input_metrics_toint(&parsed.input, (char *) string);
    plug_message_send("received_request", (void *) &parsed);
    // Return the metrics converted into text
    req->output = process_output_metrics_tochar(&parsed);
    return NULL;
}

static char *process_text(void *data)
{
    metrics_request_text_t *req = (metrics_request_text_t *) data;
    const char *string = req->input;

    // Processing domain
    if (strncasecmp(string, MET_NAME(Met), 3) == 0) return process_text_metrics(req);
    if (strncasecmp(string, MET_NAME(Act), 3) == 0) return process_text_metrics(req);
    if (strncasecmp(string, MET_NAME(All), 3) == 0) return process_text_metrics(req);
    if (strncasecmp(string, MET_NAME(Met), 3) == 0) return process_text_configuration(req);
    return NULL;
}

plug_message_receive(_received_request_text)
{
    return process_text(data);
}