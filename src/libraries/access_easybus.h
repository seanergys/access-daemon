#ifndef LIB_EASYBUS_H
#define LIB_EASYBUS_H

#include "metrics_types.h"

// Synchronous SeanerBus library to the Access Daemon. Status functions return 1
// on success, 0 on failure (see errno).

// Initializes Access Daemon connection. NULL selects the default address.
int easybus_connect(char *address);

void easybus_disconnect();

int easybus_is_connected();

// Allocate the metric catalogue (meta-data only). Both arguments are required;
// release *mets with free(). Any previous pointer is overwritten.
int easybus_get_available_metrics(metric_t **mets, int *mets_count);

// Returns 1 if the specific metric is found, and allocates allocate the metric
// meta-data if found and _met is not NULL. Release *_met with free().
int easybus_search_available_metrics(domain_e dom, metric_e met, device_e dev,
    metric_t **_met);

// Sends a Get request to the Access Daemon. All is a wildcard available for
// domain, metric, device and device number. With *mets == NULL, a copy
// installed in thread-owned storage is returned, so do no free it and consume
// the values immediately after return. Values use device indices; unselected
// slots are zero. On error 0 is returned, errno is set and the error message
// can be found in 'error_msg' pointer within the problematic metric_t.
int easybus_request_get(domain_e dom, metric_e met, device_e dev, int dev_no,
    metric_t **mets, int *mets_count);

// Sends a Set request to the Access Daemon. Device number may be All. values
// must be non-NULL: values_count must cover those indices. error_msg is NULL
// or a 256-byte buffer for daemon errors.
int easybus_request_set(metric_e met, device_e dev, int dev_no,
    uint64_t *values, int values_count, char *error_msg);

// Set one value on All or selected device; same error_msg requirements as Set.
int easybus_request_set_imm(metric_e met, device_e dev, int dev_no,
    uint64_t value, char *error_msg);

// Allocate zeroed metric/value storage for Get requests. *mets is replaced on
// success. Release with free(*mets).
int easybus_metrics_alloc(metric_t **mets);

#endif // LIB_EASYBUS_H