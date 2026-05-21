#!/bin/bash

export LD_LIBRARY_PATH=./src/common

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
./src/accessd ${1} \
    --paths=./src/plugins/general:./src/plugins/metrics \
    --plugins=keyboard.so+query_handler.so+metrics_central.so+cpu_freq_met_intel.so+cpu_pow_met_msr.so