#include <stdio.h>
#include <stdlib.h>
#include <plugins/general/topology_ext.h>
#include <management/cpufreq/cpufreq_base.h>

static topology_ext_t topo;

plug_identity()
{
    *tag       = "topology_ext";
    *tags_deps = NULL;
}

plug_init(_topology_ext)
{
    cpufreq_base_t base_freq = {0};
    topology_init((topology_t *) &topo);
    cpufreq_base_init((topology_t *) &topo, &base_freq);
    topo.base_frequency = base_freq.frequency;
    *data_alloc = &topo;
    return NULL;
}