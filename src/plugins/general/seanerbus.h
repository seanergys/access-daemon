#ifndef SEANERBUS_H
#define SEANERBUS_H

#include <uuid/uuid.h>
#include <common/metrics_types.h>
#include <seanerbus/c/seanerbus.h>

#define GET_REQ_V1      SEANERBUS_PAYLOAD_GET_REQ_V1
#define SET_REQ_V1      SEANERBUS_PAYLOAD_SET_REQ_V1
#define GET_RES_V1      SEANERBUS_PAYLOAD_GET_RES_V1
#define ERR_RES_V1      SEANERBUS_PAYLOAD_ERROR_RES_V1
#define GET_RES_V1_SIZE 16

#define SEA_LOCAL_ADDRESS "seanerbroker://localhost:5398";
#define SEA_UUIDS_COUNTER "Uuids_count"

// Short names
typedef seanerbus_name_entry_t sea_name_entry_t;
typedef seanerbus_connection_t sea_conn_t;
typedef seanerbus_entity_t     sea_addr_t;
typedef seanerbus_message_t    sea_msg_t;
typedef seanerbus_status_t     sea_stat_t;
typedef seanerbus_error_t      sea_err_t;

typedef struct sea_metric_s {
    sea_addr_t *addr;
    uuid_t      uuid;
    char        uuid_str[37];
    metric_t   *met; // Reference to the rest of the metric meta-data
    uint32_t    device_no;
    int         registered;
    uint64_t    value; // Last value
    timestamp_t timestamp;
} sea_metric_t;

#endif //SEANERBUS_H
