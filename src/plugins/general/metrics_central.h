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
#define MET_NAME(m)    #m

typedef enum {
    Ok = 0,
    Error,
} status_t;

typedef enum {
    Metrics,
    Actuators,
} domain_t;

typedef enum {
    AllDevices = DEVS_ALL,
    Cpu = 1,
    Thread = Cpu,
    Core,
    Pp0 = Core,
    Socket,
    Package = Socket,
    Uncore,
    Pp1 = Uncore,
    Dram,
    Mem = Dram,
    Gpu,
} device_t;

typedef enum {
    AllMetrics = DEVS_ALL,
    Pow = 1,
    Pwr = Pow,
    Energy,
    Freq,
    AccFreq,
} metric_t;

typedef enum {
    Get = 0,
    Set,
    Read = Get,
    Write = Set,
    GetAcc, // Gets metrics and also the derived immediates or cumulative
    Info,
} command_t;

typedef struct metrics_input_s {
    domain_t      domain;
    char          domain_name[8];
    command_t     command;
    char          command_name[8];
    device_t      device;
    char          device_name[8];
    int32_t       device_no; // currently we only accept 1 device or 'all'
    char          device_no_name[8];
    cpu_set_t     device_mask; // exploration pending
    metric_t      metric; // metric/actuator id
    char          metric_name[32];
    uint64_t     *values; // actuator values
    uint32_t      values_count;
} metrics_input_t;

typedef struct metrics_output_s {
    status_t        state;
    const char     *error_msg;
    device_t        device;
    const char     *device_name;
    int32_t         device_no;
    metric_t        metric; // id
    const char     *metric_name;
    timestamp_t     time;
    uint64_t       *values;
    uint32_t        values_count;
    const char     *units;
    device_t        granularity;
} metrics_output_t;

typedef struct request_metrics_s {
    metrics_input_t   input;
    status_t          state;
    const char       *error_msg;
    metrics_output_t *outputs;
    uint8_t           outputs_count;
} request_metrics_t;

#endif // PLUG_METRICS_CENTRAL_H