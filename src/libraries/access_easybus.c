#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <common/system/time.h>
#include <common/string_helpers.h>
#include <plugins/general/seanerbus.h>
#include <libraries/access_easybus.h>

typedef struct synchro_s {
    int *expected_count;
    int *completed_count;
    void *value;
} synchro_t;

// Metrics allocation
static pthread_t       thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static metric_t        mets[MAX_METRICS_NO];
static sea_metric_t    seas[MAX_METRICS_NO * MAX_DEVICES_NO];
static int             max_devices_no;
static int             seas_count; // Total number of UUIDs/addresses.
static int             mets_count; // Number of different metrics
static char            hostname[128];
static synchro_t       sync_global;
static int             is_connected;
static sea_conn_t     *conn;

//#define debug(...) { }printf(__VA_ARGS__); printf("\n");
#define debug(...)

#define sea(f, ...) \
    if ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK) { \
        goto cleanup; \
    }

static void *thread_message_dealer(void *data)
{
    sea_err_t *err = NULL;
    sea_stat_t st = 0;
    // This is the secondary thread that periodically checks whether there are
    // incoming messages (and invoking callbacks) or not.
    while (is_connected) {
        sea(connection_handle, conn);
        usleep(100);
    }
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(err));
        seanerbus_error_destroy(err);
    }
    return NULL;
}

void metric_build(metric_t *met, const char *key, const char *value)
{
    // This function transform strings to enums
    if (is(key, MET_NAME(Domain))) {
        met->domain = label_to_id(value);
        strcpy(met->domain_label, value);
    } else if (is(key, MET_NAME(Metric))) {
        met->metric = label_to_id(value);
        strcpy(met->metric_label, value);
    } else if (is(key, MET_NAME(Device))) {
        met->device = label_to_id(value);
        strcpy(met->device_label, value);
    } else if (is(key, MET_NAME(Device_no))) {
        met->device_no = ((uint32_t) atoi(value));
    }
}

static void named(sea_conn_t *conn, sea_stat_t st, const sea_name_entry_t *entries,
    size_t entries_count, const sea_err_t *err, void *udata)
{
    sea_metric_t *sea = udata;
    metric_t met = {0};
    int e, m;

    if (st != SEANERBUS_STATUS_OK) {
        debug("Error: %s", seanerbus_error_message(err));
        return;
    }
    // Named function is receveis lots of UUID's and labels. With the labels,
    // it reconstruct all the available metrics in the Access Daemon.
    for (e = 0; e < entries_count; ++e) {
        metric_build(&met, entries[e].key.data, entries[e].value.data);
    }
    for (m = 0; m < mets_count; ++m) {
        // If there is a metric in the list with this UUID labels, then the
        // devices counter is increased based in the device number the UUID
        // represents.
        if (mets[m].domain == met.domain &&
            mets[m].metric == met.metric &&
            mets[m].device == met.device) {
            if ((met.device_no+1) > mets[m].devs_count) {
                mets[m].devs_count = met.device_no+1;
            }
            if (mets[m].devs_count > max_devices_no) {
                // The maximum detected devices allows to save space in later
                // allocations. MAX_DEVICES_NO vs max_devices_no.
                max_devices_no = mets[m].devs_count;
            }
            break;
        }
    }
    // If the labels are not detected in the metrics list, a new one is created.
    if (m == mets_count) {
        memcpy(&mets[mets_count], &met, sizeof(metric_t));
        mets[mets_count].domain_name = mets[mets_count].domain_label;
        mets[mets_count].metric_name = mets[mets_count].metric_label;
        mets[mets_count].device_name = mets[mets_count].device_label;
        ++mets_count;
    }
    //
    sea->met = &mets[m], sea->device_no = met.device_no;
    *sync_global.completed_count += 1;
}

static void discovered(sea_conn_t *conn, sea_stat_t st, const sea_addr_t *const *addrs,
    size_t addrs_count, const sea_err_t *error, void *udata)
{
    sea_err_t *err = NULL;
    size_t size;
    int i;

    if (st != SEANERBUS_STATUS_OK) {
        debug("Error: %s", seanerbus_error_message(error));
        return;
    }
    // This function receives an array of discovered addresses that matched the
    // parameters you used in the resolve function.
    for (i = 0; i < addrs_count; ++i) {
        sea(address_clone, addrs[i], &seas[seas_count].addr);
        // Saving the UUID for debugging purposes
        sea(address_to_string, addrs[i], seas[seas_count].uuid_str, 37, &size);
        // For every UUID/address discovered, their labels are requested.
        sea(reverse_resolve, conn, seas[seas_count].addr, named, &seas[seas_count]);
        seas_count++;
    }
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(error));
        seanerbus_error_destroy(err);
    }
}

static void named_global(sea_conn_t *conn, sea_stat_t st, const sea_name_entry_t *entries,
    size_t entries_count, const sea_err_t *error, void *udata)
{
    seanerbus_name_entry_t names[2] = {0};
    sea_err_t *err = NULL;
    int e;

    if (st != SEANERBUS_STATUS_OK) {
        debug("Error: %s", seanerbus_error_message(error));
        return;
    }
    // Named Global looks for the SEA_UUIDS_COUNTER label, which contains the
    // total number of addresses, and it tells how many messages to expect.
    for (e = 0; e < entries_count; ++e) {
        if (is(entries[e].key.data, SEA_UUIDS_COUNTER)) {
            *sync_global.expected_count = (int) atoi(entries[e].value.data);
        }
    }
    // Then all the Metrics and Actuators are requested.
    names[0] = seanerbus_name_entry_from_c_strings("Hostname", hostname);
    names[1] = seanerbus_name_entry_from_c_strings("Domain", "*");
    // All the metrics and actuators, by now include the Domain label.
    sea(resolve, conn, names, 2, discovered, udata);
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(error));
        seanerbus_error_destroy(err);
    }
}

static void discovered_global(sea_conn_t *conn, sea_stat_t st, const sea_addr_t *const *addrs,
    size_t addrs_count, const sea_err_t *error, void *udata)
{
    static char uuid_str[37];
    static sea_addr_t *addr;
    sea_err_t *err = NULL;
    size_t size;

    if (st != SEANERBUS_STATUS_OK) {
        debug("Error: %s", seanerbus_error_message(error));
        return;
    }
    if (addrs_count == 0) {
        debug("Error: no addresses registered");
        return;
    }
    sea(address_clone, addrs[0], &addr);
    sea(address_to_string, addrs[0], uuid_str, 37, &size);
    // Trying to resolve the address of the SEA_UUIDS_COUNTER
    sea(reverse_resolve, conn, addr, named_global, udata);
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(error));
        seanerbus_error_destroy(err);
    }
}

static int syncronize(synchro_t *sync)
{
    timestamp_t t1;
    timestamp_get(&t1);
    // This function is used to synchronize SeanerBUS callbacks with the calling
    // functions. The Sync parameters have to last the EasyBUS calling functions,
    // and that means their addresses can not live in the stack.
    while (*sync->expected_count == 0 || *sync->expected_count != *sync->completed_count) {
        if (timestamp_diffnow(&t1, TIME_SECS) > 2LLU) {
            return 0; // Disconnected
        }
    }
    return 1; // Connected
}

int easybus_connect(char *local_address)
{
    char *address = SEA_LOCAL_ADDRESS;
    seanerbus_name_entry_t names[2];
    int completed_count = 0;
    int expected_count = 0;
    sea_err_t *err = NULL;
    sea_stat_t st = 0;

    while (pthread_mutex_trylock(&lock));
    if (is_connected) {
        goto cleanup;
    }
    gethostname(hostname, 128);
    // Initializing synchronization mechanism
    sync_global.expected_count = &expected_count;
    sync_global.completed_count = &completed_count;
    // Connecting to SeanerBUS
    sea(connection_create, address, address, &conn);
    // Getting global UUIDs counter
    names[0] = seanerbus_name_entry_from_c_strings("Hostname", hostname);
    names[1] = seanerbus_name_entry_from_c_strings(SEA_UUIDS_COUNTER, "*");
    sea(resolve, conn, names, 2, discovered_global, NULL);
    // Waiting all the data from Naming Service
    is_connected = 1;
    if ((errno = pthread_create(&thread, NULL, thread_message_dealer, NULL)) != 0) {
        is_connected = 0;
        goto cleanup;
    }
    if (!(is_connected = syncronize(&sync_global))) {
        errno = ETIMEDOUT;
    }
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(err));
        seanerbus_error_destroy(err);
        errno = ENOTCONN;
    }
    pthread_mutex_unlock(&lock);
    return is_connected;
}

void easybus_disconnect()
{
    while (pthread_mutex_trylock(&lock));
    if (is_connected) {
        seanerbus_connection_shutdown(conn, NULL);
        seanerbus_connection_destroy(conn);
        is_connected = 0;
    }
    pthread_mutex_unlock(&lock);
}

int easybus_is_connected()
{
    return is_connected;
}

int easybus_get_available_metrics(metric_t **_mets, int *_mets_count)
{
    if (!is_connected) {
        errno = ENOTCONN;
        return 0;
    }
    if (_mets == NULL || _mets_count == NULL) {
        errno = EINVAL;
        return 0;
    }
    *_mets = calloc(mets_count, sizeof(metric_t));
    *_mets = memcpy(*_mets, mets, mets_count);
    *_mets_count = mets_count;
    return 1;
}

static void received(sea_conn_t *conn, sea_stat_t st, const sea_msg_t *response,
    const sea_err_t *error, void *udata)
{
    seanerbus_byte_view_t payload;
    synchro_t *sync = udata;
    sea_err_t *err = NULL;

    if (st != SEANERBUS_STATUS_OK) {
        debug("Error: %s", seanerbus_error_message(error));
        return;
    }
    sea(message_get_payload, response, &payload);
    memcpy(sync->value, payload.data, sizeof(uint64_t));
    *sync->completed_count += 1;
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(error));
        seanerbus_error_destroy(err);
    }
}

int easybus_request(domain_e dom, metric_e met, device_e dev, command_e cmd,
    int dno, uint64_t *_values, metric_t **_mets, int *_mets_count)
{
    seanerbus_message_t *request = NULL;
    static __thread uint64_t  *tls_values[MAX_METRICS_NO] = {0};
    static __thread synchro_t *tls_sync = NULL;
    static __thread metric_t  *tls_mets = NULL;
    int tls_mets_count  = 0;
    int completed_count = 0;
    int expected_count  = 0;
    sea_err_t *err = NULL;
    sea_stat_t st = 0;
    int s, m, e;

    // Initializing the returned request for the rest of the session
    if (tls_mets == NULL) {
        tls_sync = calloc(seas_count, sizeof(synchro_t));
        tls_mets = calloc(mets_count, sizeof(metric_t));
        for (m = 0; m < mets_count; ++m) {
            tls_values[m] = calloc(max_devices_no, sizeof(uint64_t));
        }
    }
    // Looking the addresses of interest
    for (s = 0; s < seas_count; ++s) {
        if (dom != AllDomains && dom != seas[s].met->domain) { continue; }
        if (dev != AllDevices && dev != seas[s].met->device) { continue; }
        if (met != AllMetrics && met != seas[s].met->metric) { continue; }
        if (dno != All && dno != seas[s].device_no  ) { continue; }
        ++expected_count;
    }
    for (s = e = 0; s < seas_count; ++s) {
        if (dom != AllDomains && dom != seas[s].met->domain) { continue; }
        if (dev != AllDevices && dev != seas[s].met->device) { continue; }
        if (met != AllMetrics && met != seas[s].met->metric) { continue; }
        if (dno != All && dno != seas[s].device_no  ) { continue; }
        // Find if the place for the address in the outputs.
        for (m = 0; m < tls_mets_count; ++m) {
            if (tls_mets[m].domain == seas[s].met->domain &&
                tls_mets[m].device == seas[s].met->device &&
                tls_mets[m].metric == seas[s].met->metric) {
                break;
            }
        }
        // Copying the metric_t metadata if not found
        if (m == tls_mets_count) {
            memcpy(&tls_mets[m], seas[s].met, sizeof(metric_t));
            tls_mets[m].values = tls_values[m];
            ++tls_mets_count;
        }
        // Setting synchronization mechanism
        tls_sync[e].expected_count  = &expected_count;
        tls_sync[e].completed_count = &completed_count;
        tls_sync[e].value = &tls_mets[m].values[seas[s].device_no];
        // Creating the request message
        // TODO: pending Actuators
        sea(message_create, seas[s].addr, GET_REQ_V1,
                seanerbus_byte_view_from_data(NULL, 0), 0, &request);
        sea(connection_request, conn, request, received, &tls_sync[e]);
        seanerbus_message_destroy(request);
        ++e;
    }
    if (syncronize(&tls_sync[0])) {
        errno = ETIMEDOUT;
    }
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(err));
        seanerbus_error_destroy(err);
    }
    *_mets = tls_mets, *_mets_count = tls_mets_count;
    return (st == SEANERBUS_STATUS_OK) && (expected_count == completed_count);
}