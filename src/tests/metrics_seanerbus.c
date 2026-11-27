/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <plugins/general/seanerbus.h>

typedef struct metric_s {
    sea_addr_t *addr;
    char        uuid[37];
    char        domain_label[16];
    char        device_label[16];
    char        metric_label[16];
    char        device_no_label[8];
    uint64_t    value;
} protomet_t;

static protomet_t  mets[MAX_METRICS_NO*MAX_DEVICES_NO];
static int         mets_count;
static sea_conn_t *conn;
static sea_stat_t  st;
static sea_err_t  *err;

#define sea(f, ...) \
    if ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK) { \
        printf("SeanerBUS error %d during %s: %s\n", st, #f, seanerbus_error_message(err)); \
        exit(0); \
    }

static int is(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2) == 0;
}

static void discovered(sea_conn_t *conn, sea_stat_t st,
    const sea_addr_t *const *addrs, size_t addrs_count, const sea_err_t *error, void *udata)
{
    size_t size;
    int i;

    for (i = 0; i < addrs_count; ++i) {
        sea(entity_clone, addrs[i], &mets[mets_count].addr);
        sea(entity_to_string, addrs[i], mets[mets_count].uuid, 37, &size);
        printf("Discovered %s\n", mets[mets_count].uuid);
        mets_count++;
    }
}

static void named(sea_conn_t *conn, sea_stat_t st, const sea_name_entry_t *entries,
    size_t entries_count, const sea_err_t *error, void *udata)
{
    protomet_t *met = udata;
    int e;

    for (e = 0; e < entries_count; ++e) {
        if (is(entries[e].key.data, "Domain")) strcpy(met->domain_label, entries[e].value.data);
        if (is(entries[e].key.data, "Metric")) strcpy(met->metric_label, entries[e].value.data);
        if (is(entries[e].key.data, "Device")) strcpy(met->device_label, entries[e].value.data);
        if (is(entries[e].key.data, "Device_no")) strcpy(met->device_no_label, entries[e].value.data);
    }
    printf("Named %s %s:%s:%s:%s\n", met->uuid, met->domain_label, met->metric_label, met->device_label, met->device_no_label);
}

static void received(sea_conn_t *conn, sea_stat_t status,
    const sea_msg_t *response, const sea_err_t *error, void *udata)
{
    seanerbus_GetResV1_reader_t *payload;
    protomet_t *met = udata;
    uint64_t type;

    sea(message_get_payload_type, response, &type);
    printf("Received payload type: %ld\n", type);
    if (type == GET_RES_V1) {
        sea(message_as_GetResV1_reader, response, &payload);
        met->value = seanerbus_GetResV1_get_Value(payload);
        seanerbus_GetResV1_reader_destroy(payload);
    }
}

void connection_handle_and_wait_for_key()
{
    char c;
    printf("Enter to continue...\n");
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    while (read(STDIN_FILENO, &c, 1) != 1 || c != '\n') {
        seanerbus_connection_handle(conn, &err);
    }
}

int main(int argc, char* argv[])
{
    char *address = SEA_LOCAL_ADDRESS;
    seanerbus_message_t *request = NULL;
    seanerbus_name_entry_t names[3];
    int m;

    // Connecting to SeanerBUS
    sea(connection_create, address, address, &conn);
    // Looking for all Metrics and Actuators available
    names[0] = seanerbus_name_entry_from_c_strings("Domain", "*");
    sea(resolve, conn, names, 1, discovered, &mets[0]);
    connection_handle_and_wait_for_key();
    for (m = 0; m < mets_count; ++m) {
        sea(reverse_resolve, conn, mets[m].addr, named, &mets[m]);
    }
    connection_handle_and_wait_for_key();
    // Requesting values for all Metrics and Actuators
    for (m = 0; m < mets_count; ++m) {
        sea(message_create, mets[m].addr, GET_REQ_V1,
            seanerbus_byte_view_from_data(NULL, 0), 0, &request);
        sea(connection_request, conn, request, received, &mets[m]);
        seanerbus_message_destroy(request);
    }
    connection_handle_and_wait_for_key();
    for (m = 0; m < mets_count; ++m) {
        printf("%d:%s:%s:%s:%s = %lu\n", m, mets[m].domain_label,
            mets[m].metric_label, mets[m].device_label, mets[m].device_no_label,
            mets[m].value);
    }
    // Trying to set some actuator (we know the last one is an actuator)
#if 0
    value[0] = 180;
    sea(message_create, mets[mets_count-1].addr, SEANERBUS_PAYLOAD_SET_REQ_V1,
        seanerbus_byte_view_from_data(&value, sizeof(uint64_t)), 0, &request);
    sea(connection_request, conn, request, received, &mets[mets_count-1]);
    seanerbus_message_destroy(request);
    connection_handle_and_wait_for_key();
#endif
    // Closing
    seanerbus_connection_shutdown(conn, NULL);
    return 0;
}