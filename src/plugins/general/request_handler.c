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

plug_message_receive(_received_request)
{
    // More direct, no parsing needed
    return NULL;
}