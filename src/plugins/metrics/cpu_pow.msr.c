#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <common/framework.h>
#include <common/math_operations.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <common/metrics_types.h>

#define DEVS_MAX 2 // By now just package and pp0
#define DEV_PKG  0
#define DEV_PP0  1
#define METS_MAX 2
#define E        0 // Index 0 is Energy
#define P        1 // Index 1 is Power (derived)

typedef struct dev_ctx_s {
    topology_t *tp;
    uint64_t    addr_stat;
    metric_t    mets2[METS_MAX]; // Current
    metric_t    mets1[METS_MAX]; // Last
    device_e    granul;
    char       *granul_name;
} dev_ctx_t;

static topology_t      *tp;
static topology_t       tp_sockets;
static topology_t       tp_cores;
static dev_ctx_t        devs[DEVS_MAX]; // Devices meta-data and values
static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t         power_unit;
static double           units;
static int              acc_on; // Accumulator plugin is online

plug_identity()
{
    *tag       = "met.cpu_pow.intel";
    *tags_deps = "!topology_ext";
}

static void dev_read(dev_ctx_t *dev, uint32_t dev_min, uint32_t dev_max)
{
    uint64_t sensor;
    uint i;

    if (dev->addr_stat == 0x000) {
        return;
    }
    timestamp_get(&dev->mets2[E].time);
    for (i = dev_min; i < dev_max; i++) {
        // TODO: Control failed states
        msr_read(dev->tp->cpus[i].id, &sensor, sizeof(uint64_t), dev->addr_stat);
        dev->mets2[E].values[i] = (uint64_t) (((double) getbits64(sensor, 31, 0)) * units);
    }
}

static void dev_imm(dev_ctx_t *dev, uint32_t dev_min, uint32_t dev_max)
{
    double secs = 0.0;
    double diff = 0.0;
    uint i;

    // Seconds since last all. Is only required to convert from energy
    // accumulation to immediate power value.
    if (timestamp_is_zero(&dev->mets1[E].time)) {
        return;
    }
    secs = timestamp_fdiff(&dev->mets2[E].time, &dev->mets1[E].time, TIME_SECS, TIME_MSECS);
    // Given this is an accumulative register, as is the energy concept, we have
    // to make the difference between the current sample and the last one, and
    // divide by the seconds passed to get the power value.
    for (i = dev_min; i < dev_max; i++) {
        diff = (double) (dev->mets2[E].values[i] - dev->mets1[E].values[i]);
        dev->mets2[P].values[i] = (uint64_t) ceil(diff / secs);
    }
}

static void dev_copy(dev_ctx_t *dev, uint32_t dev_min, uint32_t dev_max)
{
    // Saving last values to convert to immediate
    memcpy(&dev->mets1[E].values[dev_min], &dev->mets2[E].values[dev_min],
        sizeof(uint64_t) * (dev_max - dev_min));
    dev->mets1[E].time = dev->mets2[E].time;
}

plug_init(_topology_ext)
{
    int cpu, d, m;
    uint64_t res;

    // TODO: disable by now, the PP0
    tp = (topology_t *) data;
    #define ADDR_INIT(pu, pkg, dram, pp0, pp1) { \
        power_unit  = pu;   \
        devs[DEV_PKG].addr_stat = pkg; \
        devs[DEV_PP0].addr_stat = pp0; \
    }
    if (tp->vendor == VENDOR_INTEL && tp->model >= MODEL_TIGERLAKE) {
        ADDR_INIT(0x00000606, 0x00000611, 0x619, 0x00000639, 0x641);
    } else if (tp->vendor == VENDOR_AMD && tp->family >= FAMILY_ZEN) {
        ADDR_INIT(0xc0010299, 0xc001029B, 0x000, 0xc001029A, 0x000);
    } else {
        return "[X] not compatible with this CPU";
    }
    // Getting also the topology per socket
    topology_select(tp, &tp_sockets, TPSelect.socket, TPGroup.merge, 0);
    topology_select(tp, &tp_cores  , TPSelect.core  , TPGroup.merge, 0);
    // Testing if MSR can be read
    if (state_fail(msr_test(tp, MSR_RD))) {
        return "[X] not enough privileges to read MSR";
    }
    // Opening MSRs.
    for (cpu = 0; cpu < tp_cores.cpu_count; ++cpu) {
        if (state_fail(msr_open(tp_cores.cpus[cpu].id, MSR_RD))) {
            for (cpu -= 1; cpu >= 0; --cpu) {
                msr_close(tp_cores.cpus[cpu].id);
            }
            return "[X] failed while opening MSRs";
        }
    }
    if ((state_fail(msr_read(tp_cores.cpus[0].id, &res, sizeof(res), power_unit)))) {
        return "[X] failed while reading MSRs";
    }
    units = pow(0.5, (double) getbits64(res, 12, 8));
    // More initializations
    devs[DEV_PKG].tp          = (tp->vendor == VENDOR_INTEL)? &tp_sockets: &tp_sockets;
    devs[DEV_PP0].tp          = (tp->vendor == VENDOR_INTEL)? &tp_sockets: &tp_cores;
    devs[DEV_PKG].granul      = (tp->vendor == VENDOR_INTEL)? Socket: Socket;
    devs[DEV_PP0].granul      = (tp->vendor == VENDOR_INTEL)? Socket: Core;
    devs[DEV_PKG].granul_name = (tp->vendor == VENDOR_INTEL)? MET_NAME(Socket): MET_NAME(Socket);
    devs[DEV_PP0].granul_name = (tp->vendor == VENDOR_INTEL)? MET_NAME(Socket): MET_NAME(Core);
    // Allocation and first readings
    for (m = 0; m < METS_MAX; ++m) {
        devs[DEV_PKG].mets2[m].device       = Socket;
        devs[DEV_PP0].mets2[m].device       = Core;
        devs[DEV_PKG].mets2[m].device_name  = MET_NAME(Socket);
        devs[DEV_PP0].mets2[m].device_name  = MET_NAME(Core);
        devs[DEV_PKG].mets2[m].device_alias = MET_NAME(Pkg);
        devs[DEV_PP0].mets2[m].device_alias = MET_NAME(Pp0);
        devs[DEV_PKG].mets2[m].values_count = devs[DEV_PKG].tp->cpu_count;
        devs[DEV_PP0].mets2[m].values_count = devs[DEV_PP0].tp->cpu_count;
        devs[DEV_PKG].mets2[m].granul       = devs[DEV_PKG].granul;
        devs[DEV_PP0].mets2[m].granul       = devs[DEV_PP0].granul;
        devs[DEV_PKG].mets2[m].granul_name  = devs[DEV_PKG].granul_name;
        devs[DEV_PP0].mets2[m].granul_name  = devs[DEV_PP0].granul_name;
    } for (d = 0; d < DEVS_MAX; ++d) {
        devs[d].mets2[E].domain      = Metric;
        devs[d].mets2[P].domain      = Metric;
        devs[d].mets2[E].domain_name = MET_NAME(Metric);
        devs[d].mets2[P].domain_name = MET_NAME(Metric);
        devs[d].mets2[E].values      = calloc(devs[d].tp->cpu_count, sizeof(uint64_t));
        devs[d].mets2[P].values      = calloc(devs[d].tp->cpu_count, sizeof(uint64_t)); // Space for immediate
        devs[d].mets1[E].values      = calloc(devs[d].tp->cpu_count, sizeof(uint64_t)); // Last energies read
        devs[d].mets2[E].metric      = Energy;
        devs[d].mets2[P].metric      = Power;
        devs[d].mets2[E].metric_name = MET_NAME(Energy);
        devs[d].mets2[P].metric_name = MET_NAME(Power);
        devs[d].mets2[E].units       = "J";
        devs[d].mets2[P].units       = "W";
        dev_read(&devs[d], 0, devs[d].tp->cpu_count);
        dev_copy(&devs[d], 0, devs[d].tp->cpu_count);
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

static char *mets_read(void *data, dev_ctx_t *dev, uint64_t *copy)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t          *inp = &req->input;
    metric_t          *out = &req->outputs[req->outputs_count];
    uint32_t     immediate = (inp->command == GetAcc);
    uint32_t       cpu_max = dev->tp->cpu_count;
    uint32_t       cpu_min = 0;

    if (inp->device_no >= dev->tp->cpu_count || inp->device_no < DEVS_ALL) {
        return "[X] Invalid device";
    }
    // If just one device is specified
    if (inp->device_no != All) {
        cpu_max = inp->device_no + 1;
        cpu_min = inp->device_no;
    }
    // Given the difference is only required to get the immediate value, and the
    // power is only offered when accumulation plugin is enabled, which only
    // asks for all devices in regular periods of time and does not let others
    // from reading values, this plugin does not have potential dangers.
    while (pthread_mutex_trylock(&lock));
    dev_read(dev, cpu_min, cpu_max);
    dev_imm (dev, cpu_min, cpu_max);
    dev_copy(dev, cpu_min, cpu_max);
    // Returning metric
    met_copy(&out[E], &dev->mets2[E], &copy[cpu_min], cpu_min, cpu_max);
    met_copy(&out[P], &dev->mets2[P], &copy[cpu_max], cpu_min, cpu_max * immediate);
    req->outputs_count += 1 + immediate;
    pthread_mutex_unlock(&lock);
    return NULL;
}

static char *mets_return(void *data, uint dev)
{
    static __thread uint64_t *copy[DEVS_MAX] = {0};
    // Each thread here will have its own copy of values.
    if (copy[dev] == NULL) {
        copy[dev] = calloc(devs[dev].tp->cpu_count, sizeof(uint64_t)*METS_MAX);
    }
    return mets_read(data, &devs[dev], copy[dev]);
}

plug_message_receive(_met_energy_core_get)
{
    return mets_return(data, DEV_PP0);
}

plug_message_receive(_met_energy_socket_get)
{
    return mets_return(data, DEV_PKG);
}

plug_message_receive(_met_energy_all_get)
{
    mets_return(data, DEV_PP0);
    return mets_return(data, DEV_PKG);
}

plug_message_receive(_met_all_all_get)
{
    mets_return(data, DEV_PP0);
    return mets_return(data, DEV_PKG);
}

plug_message_receive(_all_all_all_info)
{
    metrics_request_t *req = (metrics_request_t *) data;
    metric_t          *out = &req->outputs[req->outputs_count];
    met_copy(&out[0], &devs[DEV_PP0].mets2[E], NULL, 0, devs[DEV_PP0].tp->cpu_count);
    met_copy(&out[1], &devs[DEV_PKG].mets2[E], NULL, 0, devs[DEV_PKG].tp->cpu_count);
    met_copy(&out[2], &devs[DEV_PP0].mets2[P], NULL, 0, devs[DEV_PP0].tp->cpu_count * acc_on);
    met_copy(&out[3], &devs[DEV_PKG].mets2[P], NULL, 0, devs[DEV_PKG].tp->cpu_count * acc_on);
    req->outputs_count += 2 + (acc_on*2);
    return NULL;
}