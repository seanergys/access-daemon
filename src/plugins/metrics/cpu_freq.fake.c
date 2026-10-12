#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <common/framework.h>
#include <common/system/random.h>
#include <common/metrics_types.h>
#include <common/math_operations.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <plugins/general/topology_ext.h>

#define METS_MAX      2
#define F             0 // Index 0 is current Freq
#define A             1 // Index 1 is current AccFreq (derived)

static pthread_key_t   copy_key;
static int             acc_on; // Accumulator plugin is online
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static metric_t        mets[METS_MAX]; // Current and last Freq and AccFreq
static topology_t     *tp;

plug_identity()
{
    *tag       = "met.cpu_freq.fake";
    *tags_deps = "!topology_ext";
}

static void dev_read(uint32_t cpu_min, uint32_t cpu_max)
{
    int i;
    timestamp_get(&mets[F].time);
    timestamp_get(&mets[A].time);
    for (i = cpu_min; i < cpu_max; ++i) {
        mets[F].values[i]  = random_getrank64(2400000, 1000000);
    }
}

static void dev_acc(uint32_t cpu_min, uint32_t cpu_max)
{
    int i;
    for (i = cpu_min; i < cpu_max; ++i) {
        mets[A].values[i] += mets[F].values[i];
    }
}

static void dev_copy(uint32_t cpu_min, uint32_t cpu_max)
{
}

plug_init(_topology_ext)
{
    int rc, m;

    //
    tp = (topology_t *) data;
    // Allocating and meta-data
    mets[F].metric      = Freq;
    mets[F].metric_name = MET_NAME(Freq);
    mets[A].metric      = AccFreq;
    mets[A].metric_name = MET_NAME(AccFreq);
    for (m = 0; m < METS_MAX; ++m) {
        mets[m].domain       = Metric;
        mets[m].domain_name  = MET_NAME(Metric);
        mets[m].device       = Cpu;
        mets[m].device_name  = MET_NAME(Cpu);
        mets[m].values       = calloc(tp->cpu_count, sizeof(uint64_t));
        mets[m].values_count = tp->cpu_count;
        mets[m].units        = "KHz";
        mets[m].granul       = mets[m].device;
        mets[m].granul_name  = mets[m].device_name;
    }
    // First readings
    dev_read(0, tp->cpu_count);
    dev_acc (0, tp->cpu_count);
    dev_copy(0, tp->cpu_count);
    // Creating a key to clean the TLS/TSD
    if ((rc = pthread_key_create(&copy_key, free)) != 0) {
        return "[D] Failed to create CPU frequency output key";
    }
    return NULL;
}

plug_init(_metrics_accumulator)
{
    // The accumulator plugin is present so we can get accumulative values
    acc_on = 1;
    return NULL;
}

static void met_copy(metric_t *out, metric_t *met, uint64_t *copy, int cpu_min, int cpu_max)
{
    if (cpu_max == 0) return;
    memcpy(out, met, sizeof(metric_t)), out->values = NULL; // Meta-data only
    out->values_count = cpu_max - cpu_min; // #Values/devices
    if ((out->values = copy) == NULL) return; // The caller does not want values
    memcpy(copy, &met->values[cpu_min], sizeof(uint64_t)*out->values_count);
}

static char *mets_read(void *data, uint64_t *copy)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t  *out = &req->outputs[req->outputs_count];
    uint32_t   accumulated = (req->input.command == GetAcc);
    uint32_t       cpu_max = tp->cpu_count;
    uint32_t       cpu_min = 0;
    // If just one device is specified
    if (req->input.device_no != All) {
        cpu_max = req->input.device_no + 1;
        cpu_min = req->input.device_no;
    }
    // Computing the metric through sensor values. Potential dangers:
    //   - When someone calls too often, the differences tighten up, and when
    //     this happens, the frequency average that is calculated after
    //     processing the APERF and MPERF may be less precise.
    while (pthread_mutex_trylock(&lock));
    dev_read(cpu_min, cpu_max); // Getting new sensor values
    dev_acc (cpu_min, cpu_max); // The accumulation is only possible if
    dev_copy(cpu_min, cpu_max); // Copying the new values for later.
    // Returning metric
    met_copy(&out[F], &mets[F], &copy[cpu_min], cpu_min, cpu_max);
    met_copy(&out[A], &mets[A], &copy[cpu_max], cpu_min, cpu_max * accumulated);
    req->outputs_count += 1 + accumulated;
    pthread_mutex_unlock(&lock);
    return NULL;
}

static char *mets_return(void *data)
{
    uint64_t *copy;

    // Each thread owns its returned values until its next call or exit.
    copy = pthread_getspecific(copy_key);
    if (copy == NULL) {
        copy = calloc(mets[F].devs_count, sizeof(uint64_t)*METS_MAX);
        if (copy == NULL) {
            return "[S] Failed to allocate CPU frequency outputs";
        }
        if (pthread_setspecific(copy_key, copy) != 0) {
            free(copy);
            return "[S] Failed to store CPU frequency outputs";
        }
    }
    return mets_read(data, copy);
}

plug_message_receive(_met_freq_cpu_get)
{
    return mets_return(data);
}

plug_message_receive(_met_all_all_get)
{
    return mets_return(data);
}

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t  *out = &req->outputs[req->outputs_count];
    met_copy(&out[F], &mets[F], NULL, 0, tp->cpu_count);
    met_copy(&out[A], &mets[A], NULL, 0, tp->cpu_count * acc_on);
    req->outputs_count += 1 + acc_on;
    return NULL;
}
