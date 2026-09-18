#ifndef SEANERBUS_H
#define SEANERBUS_H

#include <seanerbus/c/seanerbus.h>

#define GET_REQ_V1      SEANERBUS_PAYLOAD_GET_REQ_V1
#define GET_RES_V1      SEANERBUS_PAYLOAD_GET_RES_V1
#define SET_REQ_V1      SEANERBUS_PAYLOAD_SET_REQ_V1
#define SET_RES_V1      0
#define GET_RES_V1_SIZE 16
#define SET_RES_V1_SIZE 0

// Short names
typedef seanerbus_name_entry_t sea_name_entry_t;
typedef seanerbus_connection_t sea_conn_t;
typedef seanerbus_address_t    sea_addr_t;
typedef seanerbus_message_t    sea_msg_t;
typedef seanerbus_status_t     sea_stat_t;
typedef seanerbus_error_t      sea_err_t;

#define SEA_LOCAL_ADDRESS "demobus://localhost:5398";

#endif //SEANERBUS_H
