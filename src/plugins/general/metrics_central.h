#ifndef PLUG_METRICS_CENTRAL_H
#define PLUG_METRICS_CENTRAL_H

#include <common/framework.h>
#include <plugins/general/topology_ext.h>

typedef struct met_s {
    char   *metric_name;
    int32_t device; // -1 for all devices
    void   *values;
    int8_t  values_type;
    char   *values_units;
    int32_t values_count;
    size_t  values_size; // total size
} met_t;

#endif // PLUG_METRICS_CENTRAL_H