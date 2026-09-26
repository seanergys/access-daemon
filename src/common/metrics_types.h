#ifndef METRICS_TYPES_H
#define METRICS_TYPES_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <common/system/time.h>

// This header has the general types and the common wording of the Access Daemon

#define MAX_METRICS_NO  64 //Maximum number of loadable metrics
#define DEVS_ALL       -1
#define DEVS_MASK      -2
#define MET_NAME(m)    #m

typedef enum {
    Ok = 0,
    Error,
} status_e;

typedef enum {
    All = DEVS_ALL, // Useful for the Info command
    Met,
    Metric = Met,
    Act,
    Actuator = Act,
    Cfg, // Not included in All
    Config = Cfg,
    Configuration = Cfg,
} domain_e;

typedef enum {
    AllDevices = All,
    Cpu = 1, //Logic CPUs
    Thread = Cpu,
    Core,
    Pp0 = Core,
    Socket,
    Pkg = Socket,
    Package = Socket,
    Uncore,
    Pp1 = Uncore,
    Mem,
    Dram = Mem,
    Gpu,
} device_e;

typedef enum {
    AllMetrics = All,
    Pow = 1,
    Pwr = Pow,
    Power = Pow,
    Energy,
    Freq,
    Frequency = Freq,
    AccFreq,
    FreqMem, // Pending to solve
    AccFreqMem,
    Bwidth,
    Bandwidth = Bwidth,
} metric_e;

typedef enum {
    Get = 0,
    Set,
    Read = Get,
    Write = Set,
    GetAcc, // Gets metrics and also the derived immediates or cumulative
    Info,
} command_e;

typedef struct metrics_s {
    status_e        state;
    const char     *error_msg;
    domain_e        domain;
    const char     *domain_name;
    char            domain_label[8]; // Labels are corrected names
    metric_e        metric;
    const char     *metric_name;
    char            metric_label[16];
    device_e        device;
    const char     *device_name;
    const char     *device_alias; // Secondary name
    char            device_label[16];
    int32_t         device_no;
    char            device_no_label[8];
    command_e       command;
    char            command_label[8];
    timestamp_t     time;
    uint64_t       *values; // You don't have to allocate the output values
    union {
        uint32_t    values_count;
        uint32_t    devs_count;
    };
    const char     *units;
    device_e        granul;
    const char     *granul_name;
} metric_t;

typedef struct metrics_request_s {
    metric_t     input;
    status_e     state; // General state
    const char  *error_msg; // General error
    metric_t    *outputs; // 1 input, N outputs
    uint8_t      outputs_count;
} metrics_request_t;

typedef struct request_text_s {
    char  *input;
    char  *output;
} metrics_request_text_t;

// Short-names
typedef metrics_request_text_t mets_req_text_t;
typedef metrics_request_t      mets_req_t;

#endif //METRICS_TYPES_H