#ifndef METRICS_TYPES_H
#define METRICS_TYPES_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <common/system/time.h>

// This header has the general types and the common wording of the Access Daemon

#define MAX_METRICS_NO  24 //Maximum number of loadable metrics
#define DEVS_ALL       -1
#define DEVS_MASK      -2
#define MET_NAME(m)    #m

typedef enum {
    Ok = 0,
    Error,
} status_t;

typedef enum {
    All = DEVS_ALL, // Useful for the Info command
    Met,
    Metric = Met,
    Act,
    Actuator = Act,
    Cfg, // Not included in All
    Config = Cfg,
    Configuration = Cfg,
} domain_t;

typedef enum {
    AllDevices = All,
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
    AllMetrics = All,
    Pow = 1,
    Pwr = Pow,
    Power = Pow,
    Energy,
    Freq,
    Frequency = Freq,
    AccFreq,
    FreqMem,
    AccFreqMem,
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
    metric_t      metric; // metric/actuator id
    char          metric_name[32];
    device_t      device;
    char          device_name[16];
    int32_t       device_no; // currently we only accept 1 device or 'all'
    char          device_no_name[8];
    command_t     command;
    char          command_name[8];
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
    union {
        uint64_t   *values;
        void       *values_misc;
    };
    union {
        uint32_t    values_count;
        uint32_t    devs_count;
    };
    const char     *units;
    device_t        granularity;
} metrics_output_t;

typedef struct metrics_request_s {
    metrics_input_t   input;
    status_t          state;
    const char       *error_msg;
    metrics_output_t *outputs; // 1 input, N outputs
    uint8_t           outputs_count;
} metrics_request_t;

typedef struct request_text_s {
    char  *input;
    char  *output;
} metrics_request_text_t;

// Short-names
typedef metrics_request_text_t mets_req_text_t;
typedef metrics_request_t      mets_req_t;
typedef metrics_input_t        mets_ins_t;
typedef metrics_output_t       mets_outs_t;

#endif //METRICS_TYPES_H