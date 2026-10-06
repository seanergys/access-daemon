#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <common/framework.h>
#include <common/metrics_types.h>
#include <plugins/general/seanerbus.h>

static pthread_mutex_t    lock_conn = PTHREAD_MUTEX_INITIALIZER;
static metrics_request_t  info;
static sea_metric_t      *uuids[MAX_METRICS_NO];
static int                uuids_count;
static sea_conn_t        *conn;
static int                is_connected;
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

#define sea_cb(f, ...) \
    if ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK) { \
        printf("[X] SeanerBUS error %d during %s: %s\n", \
            st, #f, seanerbus_error_message(err)); \
        goto cleanup; \
    }

plug_init(_seanerbus)
{
    const char *address = SEA_LOCAL_ADDRESS;
    sea(connection_create, address, address, &conn);
    return NULL;
}

static void print_udata(void *udata, char *label)
{
    #if 1
    sea_metric_t *uuid = udata;

    if (uuid->met == NULL) {
        return;
    }
    printf("%s %s %s_%s_%s_%d\n", label, uuid->uuid_str, uuid->met->domain_name,
        uuid->met->metric_name, uuid->met->device_name, uuid->device_no);
    #endif
}

static void registered(sea_conn_t *conn, sea_stat_t st, const sea_err_t *error, void *udata)
{
    sea_metric_t *uuid = udata;
    if (udata == NULL) {
        return;
    }
    print_udata(udata, "Registered");
    uuid->registered = (st == SEANERBUS_STATUS_OK);
}

static void unregistered(sea_conn_t *conn, sea_stat_t st, const sea_err_t *error, void *udata)
{
    sea_metric_t *uuid = udata;
    if (udata == NULL) {
        return;
    }
    print_udata(udata, "Unregistered");
    uuid->registered = (st == SEANERBUS_STATUS_OK);
}

static void sent(sea_conn_t *conn, sea_stat_t status, const sea_err_t *error, void *udata)
{
    if (udata != NULL) {
        print_udata(udata, "Sent");
    }
}

static void requested(sea_conn_t *conn, sea_stat_t st, const sea_msg_t *msg_in, const sea_err_t *error, void *udata)
{
    // We don't know how many threads SeanerBUS has, therefore the 'req' has to
    // be kept in the thread local storage.
    static __thread metrics_request_t req = {0};
    uint64_t values[2] = { 0, 0 }; // 1 by 1
    seanerbus_byte_view_t payload = {0};
    seanerbus_byte_view_t payback = {0};
    seanerbus_message_t *msg_out = NULL;
    sea_metric_t *uuid = udata;
    uint64_t type = 0;

    print_udata(udata, "Requested");
    sea_cb(message_get_payload_type, msg_in, &type);
    sea_cb(message_get_payload, msg_in, &payload);
    // Converting the SBus request into AD internal format
    req.input.domain    = uuid->met->domain;
    req.input.device    = uuid->met->device;
    req.input.metric    = uuid->met->metric;
    req.input.device_no = uuid->device_no;
    req.input.command   = (type == SET_REQ_V1)? Set: Get;
    req.input.values    = (type == SET_REQ_V1)? (uint64_t *) payload.data: NULL;
    // Sending the request to the plugins below
    plug_message_send("received_request", (void *) &req);
    if (type == GET_REQ_V1) {
        values[0] = req.outputs_count? req.outputs[0].values[0]: VALUE_ERROR;
        payback = seanerbus_byte_view_from_data(values, sizeof(uint64_t)*2);
    } else {
        payback = seanerbus_byte_view_from_data(NULL, 0);
    }
    sea_cb(message_create, uuid->addr, GET_RES_V1, payback, 0, &msg_out);
    sea_cb(connection_respond_to, conn, msg_in, msg_out, sent, udata);
cleanup:
    seanerbus_message_destroy(msg_out);
    seanerbus_error_destroy(err);
}

plug_init_post(_seanerbus)
{
    seanerbus_name_entry_t names[5];
    char hostname[128] = {0};
    char buffer[16] = {0};
    metric_t *met;
    int o, d;

    info.input.domain    = All;
    info.input.device    = All;
    info.input.metric    = All;
    info.input.device_no = All;
    info.input.command   = Info;
    plug_message_send("received_request", (void *) &info);
    // Getting the name of the node
    gethostname(hostname, 128);
    // Creating UUIDs for each end-point in the Access Daemon
    for (o = 0; o < info.outputs_count; ++o) {
        met = &info.outputs[o];
        // Allocating space for the UUIDs
        uuids[o] = calloc(met->devs_count, sizeof(sea_metric_t));
        // Creating UUIDs
        for (d = 0; d < met->devs_count; ++d) {
            // Device number to string
            uuids[o][d].met = met;
            uuids[o][d].device_no = d;
            sprintf(buffer, "%d", d);
            // Creating the address and an end-point
            #define build_endpoint(met) \
                uuid_generate_random(met.uuid); \
                uuid_unparse_lower(met.uuid, met.uuid_str); \
                sea(address_create_from_string, met.uuid_str, &met.addr); \
                sea(connection_register_endpoint, conn, met.addr, requested, &met);
            build_endpoint(uuids[o][d]);
            // Registering the UUIDs per end-point
            names[0] = seanerbus_name_entry_from_c_strings("Hostname", hostname);
            names[1] = seanerbus_name_entry_from_c_strings("Domain", met->domain_name);
            names[2] = seanerbus_name_entry_from_c_strings("Metric", met->metric_name);
            names[3] = seanerbus_name_entry_from_c_strings("Device", met->device_name);
            names[4] = seanerbus_name_entry_from_c_strings("Device_no", buffer);
            sea(register_resolve, conn, names, 5, uuids[o][d].addr, registered, &uuids[o][d]);
            ++uuids_count;
        }
    }
    // Adding one additional thing
    sprintf(buffer, "%d", uuids_count);
    uuids[o] = calloc(1, sizeof(sea_metric_t));
    build_endpoint(uuids[o][0]);
    // The number of UUIDs available in the node
    names[0] = seanerbus_name_entry_from_c_strings("Hostname", hostname);
    names[1] = seanerbus_name_entry_from_c_strings(SEA_UUIDS_COUNTER, buffer);
    sea(register_resolve, conn, names, 2, uuids[o][0].addr, registered, NULL);
    // Starting the session
    sea(connection_get_fd, conn, &fd);
    // The framework internal poll thread takes the control of that file descriptor
    plugin_manager_poll_add("seanerbus", "connection_handling", fd);
    return NULL;
}

plug_poll_attend(_connection_handling)
{
    while (pthread_mutex_trylock(&lock_conn));
    // Check connection
    sea(connection_handle, conn);
    pthread_mutex_unlock(&lock_conn);
    return NULL;
}

plug_exit()
{
    timestamp_t t1;
    int o, d;

    if (!is_connected) {
        return NULL;
    }
    seanerbus_error_destroy(err);
    for (o = 0; o < info.outputs_count; ++o) {
    for (d = 0; d < info.outputs[o].devs_count; ++d) {
        if (uuids[o][d].registered) {
            seanerbus_unregister_resolve(conn, uuids[o][d].addr, unregistered, &uuids[o][d], NULL);
        }
    }}
    // Unregistering the fake address that maintains the total number of UUIDs
    seanerbus_unregister_resolve(conn, uuids[o][0].addr, unregistered, &uuids[o][0], NULL);
    // While pending addresses to unregister (and under the timeout of 4 seconds)
    timestamp_get(&t1);
    for (o = 0; o < info.outputs_count; ++o) {
    for (d = 0; d < info.outputs[o].devs_count; ++d) {
        while (uuids[o][d].registered && timestamp_diffnow(&t1, TIME_SECS) < 4LLU) {
            sea(connection_handle, conn);
        }
    }}
    // Shutting down
    while (pthread_mutex_trylock(&lock_conn));
    seanerbus_connection_shutdown(conn, NULL);
    seanerbus_connection_destroy(conn);
    for (o = 0; o < uuids_count; ++o) {
    for (d = 0; d < info.outputs[o].devs_count; ++d) {
        seanerbus_address_destroy(uuids[o][d].addr);
    }}
    pthread_mutex_unlock(&lock_conn);
    return NULL;
}