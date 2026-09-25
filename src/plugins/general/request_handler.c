#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <plugins/general/data_plane.h>
#include <plugins/general/request_handler.h>

plug_identity()
{
    *tag       = "request_handler";
    *tags_deps = NULL;
}

plug_init(_request_handler)
{
    return NULL;
}

static char *process_request(void *data)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t *input = &req->input;

    if      (input->domain  == All    ) sprintf(input->domain_label , MET_NAME(All));
    else if (input->domain  == Met    ) sprintf(input->domain_label , MET_NAME(Met));
    else if (input->domain  == Act    ) sprintf(input->domain_label , MET_NAME(Act));
    if      (input->device  == All    ) sprintf(input->device_label , MET_NAME(All));
    else if (input->device  == Cpu    ) sprintf(input->device_label , MET_NAME(Cpu));
    else if (input->device  == Socket ) sprintf(input->device_label , MET_NAME(Socket));
    else if (input->device  == Uncore ) sprintf(input->device_label , MET_NAME(Uncore));
    else if (input->device  == Mem    ) sprintf(input->device_label , MET_NAME(Mem));
    else if (input->device  == Gpu    ) sprintf(input->device_label , MET_NAME(Gpu));
    if      (input->metric  == All    ) sprintf(input->metric_label , MET_NAME(All));
    else if (input->metric  == Pow    ) sprintf(input->metric_label , MET_NAME(Pow));
    else if (input->metric  == Energy ) sprintf(input->metric_label , MET_NAME(Energy));
    else if (input->metric  == Freq   ) sprintf(input->metric_label , MET_NAME(Freq));
    else if (input->metric  == AccFreq) sprintf(input->metric_label , MET_NAME(AccFreq));
    else if (input->metric  == Bwidth ) sprintf(input->metric_label , MET_NAME(Bwidth));
    if      (input->command == Get    ) sprintf(input->command_label, MET_NAME(Get));
    else if (input->command == Set    ) sprintf(input->command_label, MET_NAME(Set));
    else if (input->command == Info   ) sprintf(input->command_label, MET_NAME(Info));
    plug_message_send("metrics_call", data);
    return NULL;
}

plug_message_receive(_received_request)
{
    return process_request(data);
}