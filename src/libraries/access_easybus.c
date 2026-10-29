#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <common/system/time.h>
#include <common/string_helpers.h>
#include <plugins/general/seanerbus.h>
#include <libraries/access_easybus.h>

// One callback context per requested address. Counters and error storage are
// shared by all contexts belonging to the same synchronous request.
typedef struct synchro_s {
    volatile int *expected_count;
    volatile int *completed_count;
    void *output_value;
    char *error_msg;
    int  *error_no;
} synchro_t;

// Per-calling-thread storage: fallback outputs, value buffers and callback
// contexts. Caller-provided metrics replace the fallback outputs and values.
typedef struct tds_s {
    mets_req_t  req;
    uint64_t   *values;
    synchro_t  *syncs;
} tds_t;

// The discovery catalogue groups addresses by (domain, metric, device).
// Each seas entry identifies one device within a catalogue group.
static pthread_t       thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t   tds_keys[3]; // TDS, values and callback contexts.
static metric_t        mets[MAX_METRICS_NO];
static sea_metric_t    seas[MAX_METRICS_NO * MAX_DEVICES_NO];
static int             max_devices_no; // Largest discovered device index + 1.
static int             seas_count; // Number of per-device UUIDs/addresses.
static int             mets_count; // Number of catalogue groups.
static char            hostname[128];
static synchro_t       sync_global;
static int             is_connected;
static sea_conn_t     *conn;

//#define debug(...) { }printf(__VA_ARGS__); printf("\n");
#define debug(...)

#define sea_clean(f, ...) \
    if ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK) { \
        goto cleanup; \
    }

static void *thread_message_dealer(void *data)
{
    sea_err_t *err = NULL;
    sea_stat_t st = 0;
    // Drive discovery and response callbacks while callers wait synchronously.
    while (is_connected) {
        sea_clean(connection_handle, conn);
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
    // Naming labels encode both the group identity and its device index.
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
    // Recover the metric identity from this address's naming labels.
    for (e = 0; e < entries_count; ++e) {
        metric_build(&met, entries[e].key.data, entries[e].value.data);
    }
    for (m = 0; m < mets_count; ++m) {
        // Values are indexed by device number, so the required span is the
        // largest index + 1, even when discovered device numbers have gaps.
        if (mets[m].domain == met.domain &&
            mets[m].metric == met.metric &&
            mets[m].device == met.device) {
            if ((met.device_no+1) > mets[m].devs_count) {
                mets[m].devs_count = met.device_no+1;
            }
            break;
        }
    }
    // Keep one metadata entry per group; its name pointers reference its labels.
    if (m == mets_count) {
        memcpy(&mets[mets_count], &met, sizeof(metric_t));
        mets[mets_count].domain_name = mets[mets_count].domain_label;
        mets[mets_count].metric_name = mets[mets_count].metric_label;
        mets[mets_count].device_name = mets[mets_count].device_label;
        mets[mets_count].devs_count = met.device_no+1;
        ++mets_count;
    }
    // Size value buffers for the largest discovered device index.
    if (mets[m].devs_count > max_devices_no) {
        max_devices_no = mets[m].devs_count;
    }
    // Retain the shared metadata and this address's individual device index.
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
    // Discovery owns addrs only for this callback; clone addresses for requests.
    // Reverse resolution completes the catalogue asynchronously via named().
    for (i = 0; i < addrs_count; ++i) {
        sea_clean(entity_clone, addrs[i], &seas[seas_count].addr);
        // Keep the printable UUID for diagnostics.
        sea_clean(entity_to_string, addrs[i], seas[seas_count].uuid_str, 37, &size);
        // Labels identify the metric group and device represented by the UUID.
        sea_clean(reverse_resolve, conn, seas[seas_count].addr, named, &seas[seas_count]);
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
    // The daemon advertises the total number of per-device addresses here.
    // Each completed reverse lookup in named() accounts for one address.
    for (e = 0; e < entries_count; ++e) {
        if (is(entries[e].key.data, SEA_UUIDS_COUNTER)) {
            *sync_global.expected_count = (int) atoi(entries[e].value.data);
        }
    }
    // Discover all addresses carrying a Domain label for this hostname.
    names[0] = seanerbus_name_entry_from_c_strings("Hostname", hostname);
    names[1] = seanerbus_name_entry_from_c_strings("Domain", "*");
    // Metric and actuator addresses are expected to carry this label.
    sea_clean(resolve, conn, names, 2, discovered, udata);
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
    sea_clean(entity_clone, addrs[0], &addr);
    sea_clean(entity_to_string, addrs[0], uuid_str, 37, &size);
    // Resolve the counter label before starting per-device discovery.
    sea_clean(reverse_resolve, conn, addr, named_global, udata);
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
    // Callback contexts must stay valid until the last response. Their counters
    // belong to the waiting caller; zero expected responses means discovery
    // has not supplied its count yet. A timeout does not cancel pending callbacks.
    // This assumes callbacks no longer use caller-owned pointers after return.
    while (*sync->expected_count == 0 || *sync->expected_count != *sync->completed_count) {
        if (timestamp_diffnow(&t1, TIME_SECS) > 2LLU) {
            return 0; // Timed out; this does not test connection health.
        }
    }
    return 1; // All expected callbacks completed.
}

#define sea_fail(f, ...) \
    ((st = seanerbus_ ## f(__VA_ARGS__, &err)) != SEANERBUS_STATUS_OK)
#define return_errno(no) { errno = no; return 0; }

// TODO: Make the synchronization timeout configurable.
int easybus_connect(char *local_address)
{
    char *address = local_address ? local_address : SEA_LOCAL_ADDRESS;
    volatile int completed_count = 0;
    volatile int expected_count = 0;
    seanerbus_name_entry_t names[2];
    sea_err_t *err = NULL;
    sea_stat_t st = 0;

    while (pthread_mutex_trylock(&lock));
    if (is_connected) {
        goto cleanup;
    }
    // Each allocation has its own key so thread exit frees all three blocks.
    if (pthread_key_create(&tds_keys[0], free) != 0) { return 0; }
    if (pthread_key_create(&tds_keys[1], free) != 0) { return 0; }
    if (pthread_key_create(&tds_keys[2], free) != 0) { return 0; }
    gethostname(hostname, 128);
    // Discovery callbacks share these counters while connect waits.
    sync_global.expected_count = &expected_count;
    sync_global.completed_count = &completed_count;
    // Discovery and requests share this connection.
    if (sea_fail(connection_create, address, address, &conn)) {
        pthread_mutex_unlock(&lock);
        return_errno(ENOTCONN);
    }
    // Bootstrap discovery using the hostname-specific address counter.
    names[0] = seanerbus_name_entry_from_c_strings("Hostname", hostname);
    names[1] = seanerbus_name_entry_from_c_strings(SEA_UUIDS_COUNTER, "*");
    sea_clean(resolve, conn, names, 2, discovered_global, NULL);
    // Start callback dispatch before waiting for catalogue completion.
    is_connected = 1;
    if ((errno = pthread_create(&thread, NULL, thread_message_dealer, NULL)) != 0) {
        is_connected = 0;
        goto cleanup;
    }
    if (!(is_connected = syncronize(&sync_global))) {
        errno = ETIMEDOUT, is_connected = 0;
    }
    if (mets_count == 0) {
        errno = ENODATA, is_connected = 0;
    }
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(err));
        seanerbus_error_destroy(err);
    }
    if (!is_connected) {
        seanerbus_connection_shutdown(conn, NULL);
        seanerbus_connection_destroy(conn);
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

int easybus_get_available_metrics(metric_t **outputs, int *outputs_count)
{
    if (!is_connected)
        return_errno(ENOTCONN);
    if (outputs == NULL || outputs_count == NULL)
        return_errno(EINVAL);
    if ((*outputs = calloc(mets_count, sizeof(metric_t))) == NULL)
        return_errno(ENOMEM);
    *outputs = memcpy(*outputs, mets, mets_count);
    *outputs_count = mets_count;
    return 1;
}

static void received(sea_conn_t *conn, sea_stat_t st, const sea_msg_t *response,
    const sea_err_t *error, void *udata)
{
    seanerbus_ErrorResV1_reader_t *state = NULL;
    seanerbus_byte_view_t payback;
    uint64_t payback_type = 0;
    synchro_t *sync = udata;
    sea_err_t *err = NULL;

    if (st != SEANERBUS_STATUS_OK) {
        debug("Error: %s", seanerbus_error_message(error));
        *sync->error_no = ENOTCONN;
        return;
    }
    sea_clean(message_get_payload_type, response, &payback_type);
    if (payback_type == GET_RES_V1) {
        sea_clean(message_get_payload, response, &payback);
        if (payback.size != GET_RES_V1_SIZE) {
            *sync->error_no = EPROTO;
            goto cleanup;
        }
        memcpy(sync->output_value, payback.data, sizeof(uint64_t));
    } else {
        if (payback_type != ERR_RES_V1) {
            *sync->error_no = EPROTO;
            goto cleanup;
        }
        sea_clean(message_as_ErrorResV1_reader, response, &state);
        if (seanerbus_ErrorResV1_get_ErrorCode(state) != 0) {
            if (sync->error_msg != NULL) {
                // Callbacks currently run on one dispatch thread. Parallel
                // dispatch would require protecting this shared error buffer.
                snprintf(sync->error_msg, 256, "%s", seanerbus_ErrorResV1_get_What(state));
            }
            *sync->error_no = EIO;
        }
        seanerbus_ErrorResV1_reader_destroy(state);
    }
    *sync->completed_count += 1;
cleanup:
    if (err != NULL) {
        debug("Error: %s", seanerbus_error_message(error));
        seanerbus_error_destroy(err);
    }
}

static int tds_get(tds_t **_tds)
{
    tds_t *tds;
    #define tds_alloc(key, p, p_size) \
        if ((p = pthread_getspecific(key)) == NULL) { \
           if ((p = calloc(1, p_size)) != NULL) { \
               if ((errno = pthread_setspecific(key, p)) != 0) { \
                    free(p); p = NULL; return 0; \
        }} else { errno = ENOMEM; return 0; }}
    tds_alloc(tds_keys[0], tds, sizeof(tds_t));
    tds_alloc(tds_keys[1], tds->values, mets_count * max_devices_no * sizeof(uint64_t));
    tds_alloc(tds_keys[2], tds->syncs , seas_count * sizeof(synchro_t));
    *_tds = tds;
    return 1;
}

static int tds_clean(tds_t *tds)
{
    if (tds != NULL) {
        if (tds->values != NULL) free(tds->values);
        if (tds->syncs  != NULL) free(tds->syncs);
        pthread_setspecific(tds_keys[0], NULL);
        pthread_setspecific(tds_keys[1], NULL);
        pthread_setspecific(tds_keys[2], NULL);
        free(tds);
    }
    return 0;
}

static int easybus_request(domain_e dom, metric_e met, device_e dev, command_e cmd, int dno,
    uint64_t *values, int values_count, metric_t **ext_outputs, int *ext_outputs_count,
    char *error_msg)
{
    seanerbus_SetReqV1_builder_t *payload = NULL;
    seanerbus_message_t *request = NULL;
    volatile int completed_count = 0; // ¿Volatile?
    volatile int expected_count  = 0; // ¿Atomic?
    uint64_t *p_values = NULL;
    int is_get = (cmd == Get);
    metric_t *p_outs = NULL;
    sea_err_t *err = NULL;
    int is_external = 0;
    int outs_count = 0;
    tds_t *tds = NULL;
    sea_stat_t st = 0;
    int error_no = 0;
    int s, o, e, v;

    if (!is_connected)
        return_errno(ENOTCONN);
    if (cmd == Set && (dom != Act || met == AllMetrics || dev == AllDevices))
        return_errno(EINVAL);
    if (cmd == Set && (values == NULL || values_count <= 0))
        return_errno(EINVAL);
    // Callback errors are collected separately from this thread's errno.
    errno = 0;
    if (error_msg != NULL) {
        error_msg[0] = '\0';
    }
    if (!tds_get(&tds)) {
        // The errno is already set
        return tds_clean(tds);
    }
    // Count all matching addresses before dispatch: callbacks may arrive while
    // subsequent requests are still being queued.
    for (s = 0; s < seas_count; ++s) {
        if (dom != AllDomains && dom != seas[s].met->domain) { continue; }
        if (dev != AllDevices && dev != seas[s].met->device) { continue; }
        if (met != AllMetrics && met != seas[s].met->metric) { continue; }
        if (dno != All        && dno != seas[s].device_no  ) { continue; }
        ++expected_count;
    }
    if (expected_count == 0) {
        return_errno(ENODATA);
    }
    is_external = (ext_outputs != NULL &&
                  *ext_outputs != NULL &&
                  *ext_outputs != tds->req.outputs);
    if (is_get) {
        p_outs = is_external ? *ext_outputs : tds->req.outputs;
    }
    memset(tds->syncs, 0, seas_count * sizeof(synchro_t));
    // Use one callback context per matching address.
    for (s = e = 0; s < seas_count; ++s) {
        if (dom != AllDomains && dom != seas[s].met->domain) { continue; }
        if (dev != AllDevices && dev != seas[s].met->device) { continue; }
        if (met != AllMetrics && met != seas[s].met->metric) { continue; }
        if (dno != All        && dno != seas[s].device_no  ) { continue; }
        // Several device addresses can contribute to the same output group.
        for (o = 0; is_get && o < outs_count; ++o) {
            if (p_outs[o].domain == seas[s].met->domain &&
                p_outs[o].device == seas[s].met->device &&
                p_outs[o].metric == seas[s].met->metric) {
                break;
            }
        }
        // Preserve the destination values pointer before copying metadata.
        if (is_get && o == outs_count) {
            p_values = is_external? p_outs[o].values: &tds->values[o*max_devices_no];
            memcpy(&p_outs[o], seas[s].met, sizeof(metric_t));
            // Clear the full device span so unselected indices remain zero.
            memset(p_values, 0, p_outs[o].values_count * sizeof(uint64_t));
            p_outs[o].error_msg = p_outs[o].error_msg_buffer;
            p_outs[o].values = p_values;
            outs_count++;
        }
        // All callbacks share completion/error state, but write distinct values.
        tds->syncs[e].expected_count  = &expected_count;
        tds->syncs[e].completed_count = &completed_count;
        tds->syncs[e].output_value    = is_get? &p_outs[o].values[seas[s].device_no]: NULL;
        tds->syncs[e].error_msg       = is_get? (char *) p_outs[o].error_msg: error_msg;
        tds->syncs[e].error_no        = &error_no;
        // Each address accepts one Get or Set, even for an All-device request.
        if (cmd == Set) {
            // A single input value is broadcast; arrays use device indices.
            v = (values_count == 1)? 0: seas[s].device_no;
            sea_clean(SetReqV1_builder_create, &payload);
            seanerbus_SetReqV1_set_Value(payload, values[v]);
            sea_clean(SetReqV1_builder_to_message, payload, seas[s].addr, &request);
            seanerbus_SetReqV1_builder_destroy(payload);
        } else {
            sea_clean(message_create, seas[s].addr, GET_REQ_V1,
                seanerbus_byte_view_from_data(NULL, 0), 0, &request);
        }
        sea_clean(connection_request, conn, request, received, &tds->syncs[e]);
        seanerbus_message_destroy(request);
        request = NULL;
        ++e;
    }
    // Completion means that every selected address has answered.
    if (!syncronize(&tds->syncs[0])) {
        errno = ETIMEDOUT;
    }
cleanup:
    if (err != NULL) {
        if (error_msg != NULL)
            snprintf(error_msg, 256, "%s", seanerbus_error_message(err));
        else if (p_outs[0].error_msg != NULL)
            snprintf(p_outs[0].error_msg, 256, "%s", seanerbus_error_message(err));
        debug("Error: %s", seanerbus_error_message(err));
        seanerbus_error_destroy(err);
    }
    // Publish even partial outputs. The pointer assignment is required when
    // the caller passed *ext_outputs == NULL to request internal storage.
    if (error_no != 0)
        errno = error_no;
    if (ext_outputs != NULL)
        *ext_outputs = p_outs;
    if (ext_outputs_count != NULL)
        *ext_outputs_count = outs_count;
    if (st != SEANERBUS_STATUS_OK && errno == 0)
        errno = ELIBBAD;
    return (expected_count == completed_count) && (errno == 0);
}

int easybus_request_get(domain_e dom, metric_e met, device_e dev, int dev_no,
    metric_t **mets, int *mets_count)
{
    return easybus_request(dom, met, dev, Get, dev_no, NULL, 0, mets, mets_count, NULL);
}

int easybus_request_set(metric_e met, device_e dev, int dev_no,
    uint64_t *values, int values_count, char *error_msg)
{
    return easybus_request(Act, met, dev, Set, dev_no, values, values_count, NULL, NULL, error_msg);
}

int easybus_request_set_imm(metric_e met, device_e dev, int dev_no,
    uint64_t value, char *error_msg)
{
    return easybus_request(Act, met, dev, Set, dev_no, &value, 1, NULL, NULL, error_msg);
}

int easybus_metrics_alloc(metric_t **ext_outputs)
{
    metric_t *outputs;
    uint64_t *values;
    int o;

    if (!is_connected)
        return_errno(ENOTCONN);
    if (ext_outputs == NULL)
        return_errno(EINVAL);
    if ((outputs = calloc(mets_count, sizeof(metric_t) + max_devices_no * sizeof(uint64_t))) == NULL) {
        return_errno(ENOMEM);
    }
    // One allocation owns both regions; each metric receives an equally sized
    // device span, since request filters can change which group occupies a slot.
    values = (uint64_t *) (outputs + mets_count);
    for (o = 0; o < mets_count; ++o) {
        outputs[o].values = &values[o*max_devices_no];
    }
    *ext_outputs = outputs;
    return 1;
}
