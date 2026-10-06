#ifndef LIB_EASYBUS_H
#define LIB_EASYBUS_H

#include "metrics_types.h"

// This header contains the definitions of EasyBUS library functions, which
// simplifies communications with the Access Daemon through SeanerBUS.
int easybus_connect(char *address);

void easybus_disconnect();

int easybus_is_connected();

int easybus_get_available_metrics(metric_t **mets, int *mets_count);

int easybus_request(domain_e dom, metric_e met, device_e dev, command_e cmd,
    int dev_no, uint64_t *values, metric_t **mets, int *mets_count);

#endif // LIB_EASYBUS_H