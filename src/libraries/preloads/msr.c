#include <time.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <metrics/common/msr.h>

#define CPUS_MAX                  4096
#define LIMITS_COUNT              4
#define NS_PER_SEC                1000000000ULL
#define COUNTER_BASE              1000000ULL
#define ENERGY_SCALE              ((1ULL  << 14))
#define RAPL_UNITS                ((10ULL << 16) | (14ULL << 8) | 3ULL)
#define POWER_LIMIT(watts)        ((1ULL  << 15) | ((watts) * 8ULL))
#define POWER_INFO(tdp, min, max) (((max) * 8ULL << 32) | ((min) * 8ULL << 16) | ((tdp) * 8ULL))
#define PRELOAD_EXPORT            __attribute__((visibility("default")))

typedef struct sim_reg_s {
    off_t offset;
    uint64_t value;
    uint64_t rate; // Raw counter ticks per second; zero for static registers.
    uint64_t mask;
    int limit; // Writable power-limit slot, or -1 for read-only registers.
} sim_reg_t;

typedef struct sim_cpu_s {
    uint64_t limits[LIMITS_COUNT];
    uint32_t opens;
    mode_t mode;
    int initialized;
} sim_cpu_t;

// Registers reached by src/ directly, cpufreq_base and the cpupow backends.
// RAPL uses 1/8 W, 1/16384 J and 1/1024 s. Energy wraps at 32 bits;
// APERF/MPERF wrap at 64 bits and yield 120% of the host base frequency.
static const sim_reg_t regs[] = {
    { 0x000000e7, COUNTER_BASE,                      2400000000ULL, UINT64_MAX, -1 }, // MPERF
    { 0x000000e8, COUNTER_BASE,                      2880000000ULL, UINT64_MAX, -1 }, // APERF
    { 0x00000606, RAPL_UNITS,                                    0,          0, -1 },
    { 0x00000610, POWER_LIMIT(150),                              0,          0,  0 }, // PKG
    { 0x00000611, COUNTER_BASE,                 120 * ENERGY_SCALE, UINT32_MAX, -1 },
    { 0x00000614, POWER_INFO(150, 30, 250),                      0,          0, -1 },
    { 0x00000618, POWER_LIMIT(40),                               0,          0,  1 }, // DRAM
    { 0x00000619, COUNTER_BASE,                  20 * ENERGY_SCALE, UINT32_MAX, -1 },
    { 0x0000061c, POWER_INFO(40, 10, 80),                        0,          0, -1 },
    { 0x00000638, POWER_LIMIT(100),                              0,          0,  2 }, // PP0
    { 0x00000639, COUNTER_BASE,                  80 * ENERGY_SCALE, UINT32_MAX, -1 },
    { 0x00000640, POWER_LIMIT(25),                               0,          0,  3 }, // PP1
    { 0x00000641, COUNTER_BASE,                  10 * ENERGY_SCALE, UINT32_MAX, -1 },
    { 0xc0010015,                            0,                  0,          0, -1 }, // Boost enabled
    // P0 enabled: FID=24, DID=2 (2.4 GHz on pre-Zen5, 2.68 GHz on Zen5).
    { 0xc0010064, (1ULL << 63) | (2 << 8) | 24,                  0,          0, -1 },
    { 0xc0010299, RAPL_UNITS,                                    0,          0, -1 },
    { 0xc001029a, COUNTER_BASE,                  15 * ENERGY_SCALE, UINT32_MAX, -1 }, // AMD core
    { 0xc001029b, COUNTER_BASE,                 120 * ENERGY_SCALE, UINT32_MAX, -1 }, // AMD package
};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static sim_cpu_t       cpus[CPUS_MAX];
static struct timespec start;
static int             started;

static const sim_reg_t *find_reg(off_t offset)
{
    size_t i;

    for (i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        if (regs[i].offset == offset) return &regs[i];
    }
    return NULL;
}

PRELOAD_EXPORT state_t msr_test(topology_t *tp, mode_t mode)
{
    int i;

    if (!tp || !tp->initialized) {
        return_msg(EAR_ERROR, "MSR simulator topology is not initialized");
    }
    if (mode != MSR_RD && mode != MSR_WR) {
        return_msg(EAR_ERROR, "MSR simulator invalid mode");
    }
    if (tp->cpu_count <= 0 || tp->cpu_count > CPUS_MAX || !tp->cpus) {
        return_msg(EAR_ERROR, "MSR simulator invalid topology");
    }
    for (i = 0; i < tp->cpu_count; ++i) {
        if (tp->cpus[i].id < 0 || tp->cpus[i].id >= CPUS_MAX) {
            return_msg(EAR_ERROR, "MSR simulator CPU is out of range");
        }
    }
    return EAR_SUCCESS;
}

PRELOAD_EXPORT state_t msr_open(uint cpu, mode_t mode)
{
    size_t i;

    if (cpu >= CPUS_MAX || (mode != MSR_RD && mode != MSR_WR)) {
        return_msg(EAR_ERROR, "MSR simulator invalid CPU or mode");
    }
    if (pthread_mutex_lock(&lock) != 0) {
        return_msg(EAR_ERROR, "MSR simulator lock failed");
    }
    if (!started && clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        pthread_mutex_unlock(&lock);
        return_msg(EAR_ERROR, "MSR simulator clock failed");
    }
    started = 1;
    if (!cpus[cpu].initialized) {
        for (i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
            if (regs[i].limit >= 0) {
                cpus[cpu].limits[regs[i].limit] = regs[i].value;
            }
        }
        cpus[cpu].initialized = 1;
    }
    if (!cpus[cpu].opens || mode == MSR_WR) cpus[cpu].mode = mode;
    ++cpus[cpu].opens;
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

PRELOAD_EXPORT state_t msr_close(uint cpu)
{
    if (cpu >= CPUS_MAX) {
        return_msg(EAR_ERROR, "MSR simulator CPU is out of range");
    }
    if (pthread_mutex_lock(&lock) != 0) {
        return_msg(EAR_ERROR, "MSR simulator lock failed");
    }
    if (cpus[cpu].opens) --cpus[cpu].opens;
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}

static state_t read_value(const sim_reg_t *reg, uint cpu, uint64_t *value)
{
    struct timespec now;
    uint64_t secs;
    int64_t nsecs;

    *value = reg->limit >= 0 ? cpus[cpu].limits[reg->limit] : reg->value;
    if (!reg->rate) return EAR_SUCCESS;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return_msg(EAR_ERROR, "MSR simulator clock failed");
    }
    secs = now.tv_sec - start.tv_sec;
    nsecs = now.tv_nsec - start.tv_nsec;
    if (nsecs < 0) {
        --secs;
        nsecs += NS_PER_SEC;
    }
    *value = (*value + secs * reg->rate + nsecs * reg->rate / NS_PER_SEC)
           & reg->mask;
    return EAR_SUCCESS;
}

PRELOAD_EXPORT state_t msr_read(uint cpu, void *buffer, size_t size, off_t offset)
{
    const sim_reg_t *reg;
    uint64_t value;
    state_t state;

    if (cpu >= CPUS_MAX || !buffer || size != sizeof(value)) {
        return_msg(EAR_ERROR, "MSR simulator invalid read arguments");
    }
    if ((reg = find_reg(offset)) == NULL) {
        return_msg(EAR_ERROR, "MSR simulator unsupported register");
    }
    if (pthread_mutex_lock(&lock) != 0) {
        return_msg(EAR_ERROR, "MSR simulator lock failed");
    }
    if (!cpus[cpu].opens) {
        pthread_mutex_unlock(&lock);
        return_msg(EAR_ERROR, "MSR simulator CPU is not initialized");
    }
    state = read_value(reg, cpu, &value);
    if (state_ok(state)) memcpy(buffer, &value, sizeof(value));
    pthread_mutex_unlock(&lock);
    return state;
}

PRELOAD_EXPORT state_t msr_write(uint cpu, const void *buffer, size_t size, off_t offset)
{
    const sim_reg_t *reg;

    if (cpu >= CPUS_MAX || !buffer || size != sizeof(uint64_t)) {
        return_msg(EAR_ERROR, "MSR simulator invalid write arguments");
    }
    if ((reg = find_reg(offset)) == NULL || reg->limit < 0) {
        return_msg(EAR_ERROR, "MSR simulator register is not writable");
    }
    if (pthread_mutex_lock(&lock) != 0) {
        return_msg(EAR_ERROR, "MSR simulator lock failed");
    }
    if (!cpus[cpu].opens || cpus[cpu].mode != MSR_WR) {
        pthread_mutex_unlock(&lock);
        return_msg(EAR_ERROR, "MSR simulator CPU is not open for writing");
    }
    memcpy(&cpus[cpu].limits[reg->limit], buffer, size);
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}
