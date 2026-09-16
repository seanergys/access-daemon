#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <common/framework.h>
#include <common/metrics_types.h>

typedef struct request_text_s {
    char  *input;
    char  *output;
} request_text_t;

#endif // REQUEST_HANDLER_H