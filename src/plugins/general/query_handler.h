#ifndef PLUG_QUERY_HANDLER_H
#define PLUG_QUERY_HANDLER_H

#include <common/framework.h>

typedef struct answer_s {
    void    *bytes; // Here will be the proto buffer
    size_t   size;
    uint8_t  invalid;
} answer_t;

typedef struct query_s {
    char     domain[8]; //met/act
    char     metric[32];
    char     command[8];
    char     device[8];
    char     sub_device[8];
    char    *arguments; // values, but it can hold other things like sub-device
    answer_t answer;
} query_t;

#endif // PLUG_QUERY_HANDLER_H