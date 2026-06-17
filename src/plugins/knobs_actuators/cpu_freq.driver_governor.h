#ifndef ACTUATOR_CPUFREQ_DRIVER_GOVERNOR_H
#define ACTUATOR_CPUFREQ_DRIVER_GOVERNOR_H

struct governor_s {
    char *conservative;
    char *performance;
    char *userspace;
    char *powersave;
    char *ondemand;
} Governor __attribute__((weak)) = {
    .conservative = "conservative",
    .performance  = "performance",
    .userspace    = "userspace",
    .powersave    = "powersave",
    .ondemand     = "ondemand",
};

int cpu_freq_is_userspace_available();

char *cpu_freq_governor_get(int cpu, char *buffer);

int cpu_freq_governor_set(int cpu, char *governor);

int cpu_freq_governor_reset(int cpu);

#endif // ACTUATOR_CPUFREQ_DRIVER_GOVERNOR_H