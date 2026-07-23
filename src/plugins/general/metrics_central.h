#ifndef PLUG_METRICS_CENTRAL_H
#define PLUG_METRICS_CENTRAL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <common/framework.h>
#include <plugins/general/topology_ext.h>

#define MAX_METRICS_NO  24 //Maximum number of loadable metrics
#define DEVS_ALL       -1
#define DEVS_MASK      -2

typedef enum {
    Metrics,
    Actuators,
} domain_t;

typedef enum {
    Cpu,
    Core = Cpu,
    Thread = Cpu,
    Pp0 = Cpu,
    Socket,
    Package = Socket,
    Uncore,
    Pp1 = Uncore,
    Dram,
    Mem = Dram,
    Gpu,
} device_t;

typedef enum {
    Pow,
    Freq,
} metric_t;

typedef enum {
    Get,
    Set,
    Read = Get,
    Write = Set,
    Info,
} command_t;

typedef enum {
    Ok,
    Error,
} status_t;

typedef struct metrics_input_s {
    domain_t  domain;
    char      domain_name[8];
    metric_t  metric;
    char      metric_name[32];
    command_t command;
    char      command_name[8];
    device_t  device;
    char      device_name[8];
    int32_t   device_no; // currently we only accept 1 device or 'all'
    char      device_no_name[8];
    cpu_set_t device_mask; // exploration pending
    uint64_t *values;
    uint32_t  values_count;
} metrics_input_t;

typedef struct metrics_output_s {
    status_t     state;
    const char  *error_msg;
    metric_t     metric;
    const char  *metric_name;
    device_t     device;
    const char  *device_name;
    int32_t      device_no;
    uint64_t    *values;
    int32_t      values_count;
    device_t     granularity;
    const char  *units;
} metrics_output_t;

typedef struct request_metrics_s {
    metrics_input_t   input;
    metrics_output_t *outputs;
    uint8_t           outputs_count;
} request_metrics_t;

#endif // PLUG_METRICS_CENTRAL_H