#ifndef PLUG_METRICS_CENTRAL_H
#define PLUG_METRICS_CENTRAL_H

#include <common/framework.h>
#include <plugins/general/topology_ext.h>

// met_t for readings, including reading the actuators
// act_t for writings, it only includes the actuators

typedef struct met_s {
    char   *metric_name;
    int32_t device; // -1 for all devices
    char   *sub_device;
    void   *values;
    int8_t  values_type;
    char   *values_units;
    int32_t values_count;
    size_t  values_size; // total size
} met_t;

typedef struct act_s {
    int32_t   device;
    char     *sub_device;
    uint64_t *const values;
    uint32_t  values_count;
} act_t;

#endif // PLUG_METRICS_CENTRAL_H