#!/bin/bash

export LD_LIBRARY_PATH=./src/common

PLUGINS="keyboard.so"
PLUGINS+="+query_handler.so"
PLUGINS+="+metrics_central.so"
PLUGINS+="+cpu_freq_met_msr.so"
PLUGINS+="+cpu_freq_act_acpi.so"
PLUGINS+="+cpu_pow_met_msr.so"

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
./src/accessd ${1} \
    --paths=./src/plugins/general:./src/plugins/metrics \
    --plugins=${PLUGINS}