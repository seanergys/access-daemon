#ifndef PLUG_QUERY_HANDLER_H
#define PLUG_QUERY_HANDLER_H

#include <common/framework.h>

typedef struct query_s {
    char     metric[32];
    char     domain[8];
    char     device[8];
    char     command[8];
    char    *input_values;
    void    *answer; // Here will be the proto buffer
    size_t   answer_size;
} query_t;

#endif // PLUG_QUERY_HANDLER_H