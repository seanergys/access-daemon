/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/string_helpers.h>
#include <plugins/general/request_handler.h>
#include <plugins/general/request_handler_text.h>

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

    memset(output_buffer, 0, 4096);
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
            if (parsed->outputs[o].state != Ok) {
                c += sprintf(&output_buffer[c], "(error: %s) ", parsed->outputs[o].error_msg);
            }
        }
        c += sprintf(&output_buffer[c], "\n");
    }
    return output_buffer;
}

static void process_input_metrics_toint(metric_t *input, char *arguments)
{
    char buffer[128];

    input->domain  = label_to_id(input->domain_label);
    input->device  = label_to_id(input->device_label);
    input->metric  = label_to_id(input->metric_label);
    input->command = label_to_id(input->command_label);
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