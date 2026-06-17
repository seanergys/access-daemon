#ifndef PLUG_QUERY_HANDLER_H
#define PLUG_QUERY_HANDLER_H

#include <uuid/uuid.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <common/framework.h>

typedef struct ip_addr_s {
    int family; // AF_INET o AF_INET6
    union {
        struct in_addr  v4;
        struct in6_addr v6;
    } addr;
} ip_addr_t;

typedef struct comp_s {
    uuid_t    uuid;
    ip_addr_t ip;
    int       local;
} comp_t;

typedef struct answer_s {
    void    *bytes; // Here will be the proto buffer
    size_t   size;
    uint8_t  invalid;
    char    *error_msg;
} answer_t;

typedef struct query_s {
    comp_t  *component; // Request sender
    char     domain[8]; //met/act
    char     metric[32];
    char     command[8];
    char     device[8];
    char     sub_device[8];
    char    *arguments; // values, but it can hold other things like sub-device
    answer_t answer;
} query_t;

#endif // PLUG_QUERY_HANDLER_H