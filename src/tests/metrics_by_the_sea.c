#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <plugins/general/seanerbus.h>

#define METS_MAX 128
#define DEVS_MAX 1024

typedef struct metric_s {
    sea_addr_t *addr;
    char        uuid[37];
    char        domain_label[16];
    char        device_label[16];
    char        metric_label[16];
    char        device_no_label[8];
    uint64_t    value;
} protomet_t;

static protomet_t  mets[METS_MAX*DEVS_MAX];
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
        sea(address_clone, addrs[i], &mets[mets_count].addr);
        sea(address_to_string, addrs[i], mets[mets_count].uuid, 37, &size);
        printf("Discovered %s\n", mets[mets_count].uuid);
        mets_count++;
    }
}

static void named(sea_conn_t *conn, sea_stat_t st, const sea_name_entry_t *entries,
    size_t entries_count, const sea_err_t *error, void *udata)
{
    protomet_t *met = udata;
    int e, m;

    for (e = 0; e < entries_count; ++e) {
        if (is(entries[e].key.data, "domain")) strcpy(met->domain_label, entries[e].value.data);
        if (is(entries[e].key.data, "metric")) strcpy(met->metric_label, entries[e].value.data);
        if (is(entries[e].key.data, "device")) strcpy(met->device_label, entries[e].value.data);
        if (is(entries[e].key.data, "device_no")) strcpy(met->device_no_label, entries[e].value.data);
    }
    printf("Named %s %s:%s:%s:%s\n", met->uuid, met->domain_label, met->metric_label, met->device_label, met->device_no_label);
}

static void received(sea_conn_t *conn, sea_stat_t status,
    const sea_msg_t *response, const sea_err_t *error, void *udata)
{
    seanerbus_byte_view_t payload;
    protomet_t *met = udata;
    uint64_t type;

    sea(message_get_payload_type, response, &type);
    sea(message_get_payload, response, &payload);
    printf("Received payload type: %ld\n", type);
    met->value = (uint64_t) payload.data;
    memcpy(&met->value, payload.data, sizeof(met->value));
}

int connection_handle_and_wait_for_key()
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

    sea(connection_create, address, address, &conn);
    names[0] = seanerbus_name_entry_from_c_strings("metric", "*");
    sea(resolve, conn, names, 1, discovered, &mets[0]);
    connection_handle_and_wait_for_key();
    for (m = 0; m < mets_count; ++m) {
        sea(reverse_resolve, conn, mets[m].addr, named, &mets[m]);
    }
    connection_handle_and_wait_for_key();
    for (m = 0; m < mets_count; ++m) {
        sea(message_create, mets[m].addr, GET_REQ_V1,
            seanerbus_byte_view_from_data(NULL, 0), 0, &request);
        sea(connection_request, conn, request, received, &mets[m]);
        seanerbus_message_destroy(request);
    }
    // Destroys pending
    connection_handle_and_wait_for_key();
    for (m = 0; m < mets_count; ++m) {
        printf("%s:%s:%s = %lu\n", mets[m].metric_label,
            mets[m].device_label, mets[m].device_no_label, mets[m].value);
    }
    seanerbus_connection_shutdown(conn, NULL);
    return 0;
}