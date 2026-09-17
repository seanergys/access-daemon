#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <plugins/general/seanerbus.h>

typedef struct {
    sea_addr_t  *addr;
    char         uuid_str[37];  // 36 caracteres + '\0'
    uuid_t       uuid;
    mets_outs_t *met;
    uint32_t     device_no;
} uuid_full_t;

static metrics_request_t  info;
static uuid_full_t       *uuids[MAX_METRICS_NO];
static sea_conn_t        *conn;
static sea_stat_t         st;
static sea_err_t         *err;
static int                fd;

plug_identity()
{
    *tag = "seanerbus";
    *tags_deps = NULL;
}

#define sea(f, ...) \
    if ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK) { \
        return rsprintf("[X] SeanerBUS error %d during %s: %s\n", \
            st, #f, seanerbus_error_message(err)); \
    }

#define sea2(f, ...) \
    if ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK) { \
        printf("[X] SeanerBUS error %d during %s: %s\n", \
            st, #f, seanerbus_error_message(err)); \
    }

plug_init(_seanerbus)
{
    const char *address = SEA_LOCAL_ADDRESS;
    sea(connection_create, address, address, &conn);
    return NULL;
}

static void completed(sea_conn_t *conn, sea_stat_t st, const sea_err_t *error, void *udata)
{
    fprintf(stderr, ".");
}

static void sent(sea_conn_t *conn, sea_stat_t status, const sea_err_t *error, void *userdata)
{
}

static void requested(sea_conn_t *conn, sea_stat_t st, const sea_msg_t *msg_in, const sea_err_t *error, void *udata)
{
    static uint64_t fake_metric = 1;
    seanerbus_byte_view_t payload = {0};
    seanerbus_byte_view_t payback = {0};
    seanerbus_message_t *msg_out = NULL;
    uuid_full_t *uuid = udata;
    uint64_t type = 0;

    sea2(message_get_payload_type, msg_in, &type);
    sea2(message_get_payload, msg_in, &payload);
    printf("Received a request %ld related with %s_%s_%d)\n", type,
        uuid->met->metric_name, uuid->met->device_name, uuid->device_no);

    payback = seanerbus_byte_view_from_data(&fake_metric, sizeof(uint64_t));
    sea2(message_create, uuid->addr, GET_RES_V1, payback, 0, &msg_out);
    sea2(connection_respond_to, conn, msg_in, msg_out, sent, udata);
    ++fake_metric;

    seanerbus_message_destroy(msg_out);
    seanerbus_error_destroy(err);
}

plug_init_post(_seanerbus)
{
    seanerbus_name_entry_t names[3];
    metrics_output_t *met;
    char device_no[16];
    int o, d;

    info.input.domain    = All;
    info.input.device    = All;
    info.input.metric    = All;
    info.input.device_no = All;
    info.input.command   = Info;
    plug_message_send("received_request", (void *) &info);

    // Creating UUIDs
    for (o = 0; o < info.outputs_count; ++o) {
        met = &info.outputs[o];
        printf("%s_%s (%s): %d\n",
            met->device_name, met->metric_name, met->units, met->devs_count);
        // Allocating space for the UUIDs
        uuids[o] = calloc(met->devs_count, sizeof(uuid_full_t));
        // Creating UUIDs
        for (d = 0; d < met->devs_count; ++d) {
            uuid_generate_random(uuids[o][d].uuid);
            uuid_unparse_lower(uuids[o][d].uuid, uuids[o][d].uuid_str);
            // Device number treatment
            uuids[o][d].met = met;
            uuids[o][d].device_no = d;
            sprintf(device_no, "%d", d);
            printf("\t%s (%s %s %s)\n", uuids[o][d].uuid_str,
                met->metric_name, met->device_name, device_no);
            // Registering the UUIDs per end-point
            sea(address_create_from_string, uuids[o][d].uuid_str, &uuids[o][d].addr);
            sea(connection_register_endpoint, conn, uuids[o][d].addr, requested, &uuids[o][d]);
            names[0] = seanerbus_name_entry_from_c_strings("metric", met->metric_name);
            names[1] = seanerbus_name_entry_from_c_strings("device", met->device_name);
            names[2] = seanerbus_name_entry_from_c_strings("device_no", device_no);
            sea(register_resolve, conn, names, 3, uuids[o][d].addr, completed, &uuids[o][d]);
        }
    }
    sea(connection_get_fd, conn, &fd);
    // The framework internal poll thread takes the control of that file descriptor
    plugin_manager_poll_add("seanerbus", "connection_handling", fd);
    return NULL;
}

plug_poll_attend(_connection_handling)
{
    sea(connection_handle, conn);
    return NULL;
}