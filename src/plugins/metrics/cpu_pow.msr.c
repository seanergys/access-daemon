#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <common/framework.h>
#include <common/math_operations.h>
#include <common/hardware/bithack.h>
#include <metrics/common/msr.h>
#include <metrics/cpufreq/archs/intel63.h>
#include <plugins/general/metrics_central.h>

struct addr_s {
    uint64_t power_unit;
    uint64_t pkg_stat;
    uint64_t dram_stat;
    uint64_t pp0_stat; //core
    uint64_t pp1_stat;
};

#define ADDR_INIT(pu, pkg, dram, pp0, pp1) \
    {.power_unit  = pu,   \
     .pkg_stat    = pkg,  \
     .dram_stat   = dram, \
     .pp0_stat    = pp0,  \
     .pp1_stat    = pp1}

static struct addr_s addrs[] = {
    ADDR_INIT(0x606, 0x611, 0x619, 0x639, 0x641),
    ADDR_INIT(0xc0010299, 0xc001029B, 0x000, 0xc001029A, 0x000),
    {0},
};

static topology_t  tp;
static uint        arch_index;
static double      units;
static uint64_t    sensors1[128];
static uint64_t    sensors2[128];
static double      power[128];
static timestamp_t sensors1_time;
static timestamp_t sensors2_time;

plug_identity()
{
    *tag       = "met.cpu_pow.intel";
    *tags_deps = "!topology_ext";
}

static void get(uint32_t cpu_min, uint32_t cpu_max)
{
    uint64_t *addr;
    uint c, a;

    timestamp_get(&sensors2_time);
    for (c = cpu_min; c < cpu_max; c++) {
        addr = (uint64_t *) &addrs[arch_index].pkg_stat;
        for (a = 0; a < 4; a++) {
            if (*addr == 0x000) {
                continue;
            }
            msr_read(tp.cpus[c].id, &sensors2[c*4+a], sizeof(uint64_t), *addr);
            sensors2[c*4+a] = getbits64(sensors2[c*4+a], 31, 0);
            ++addr;
        }
    }
}

static void diff(uint32_t cpu_min, uint32_t cpu_max)
{
    uint32_t c, a;
    double secs;

    secs = timestamp_fdiff(&sensors2_time, &sensors1_time, TIME_SECS, TIME_MSECS);
    for (c = cpu_min; c < cpu_max; c++) {
        for (a = 0; a < 4; a++) {
            power[c*4+a] = overflow_magic_u64(sensors2[c*4+a], sensors1[c*4+a], MAXBITS32);
            power[c*4+a] = (((double) power[c*4+a]) * units) / secs;
        }
    }
}

static void copy(uint32_t cpu_min, uint32_t cpu_max)
{
    sensors1_time = sensors2_time;
    memcpy(&sensors1[cpu_min], &sensors2[cpu_min], sizeof(uint64_t) * 4 * (cpu_max - cpu_min));
}

plug_init(_topology_ext)
{
    uint64_t res;
    int cpu;

    topology_t *tp_in = (topology_t *) data;
    if (tp_in->vendor == VENDOR_INTEL && tp_in->model >= MODEL_TIGERLAKE) {
        arch_index = 0;
    } else if (tp_in->vendor == VENDOR_AMD && tp_in->family >= FAMILY_ZEN) {
        arch_index = 1;
    } else {
        return "[X] not compatible with this CPU";
    }
    // Topology now is per socket
    topology_select(tp_in, &tp, TPSelect.socket, TPGroup.merge, 0);
    // Testing if MSR can be read
    if (state_fail(msr_test(&tp, MSR_RD))) {
        return "[X] not enough privileges to read MSR";
    }
    // Opening MSRs.
    for (cpu = 0; cpu < tp.cpu_count; ++cpu) {
        if (state_fail(msr_open(tp.cpus[cpu].id, MSR_RD))) {
            for (cpu -= 1; cpu >= 0; --cpu) {
                msr_close(tp.cpus[cpu].id);
            }
            return "[X] failed while opening MSRs";
        }
    }
    if ((state_fail(msr_read(tp.cpus[0].id, &res, sizeof(res), addrs[arch_index].power_unit)))) {
        return "[X] failed while reading MSRs";
    }
    units = pow(0.5, (double) getbits64(res, 12, 8));
    get (0, tp.cpu_count);
    copy(0, tp.cpu_count);
    return NULL;
}

static char *return_metrics(void *data)
{
    met_t *met = (met_t *) data;
    uint32_t cpu_max = tp.cpu_count;
    uint32_t cpu_min = 0;

    // If just one device is specified
    if (met->device != -1) {
        cpu_max = met->device + 1;
        cpu_min = met->device;
    }
    // Computing the metric through sensor values
    get (cpu_min, cpu_max); // Getting new sensor values
    diff(cpu_min, cpu_max); // Making the difference with the previous values
    copy(cpu_min, cpu_max); // Copying the new values to the last ones array
    // Returning metric
    met->metric_name  = "cpu_pow";
    met->values       = &power[cpu_min];
    met->values_count = cpu_max - cpu_min;
    met->values_size  = sizeof(double) * met->values_count;
    met->values_type  = ID_DOUBLE;
    met->values_units = "W";
    return NULL;
}

plug_message_receive(_met_cpu_pow_get)
{
    return return_metrics(data);
}

plug_message_receive(_met_all_get)
{
    return return_metrics(data);
}