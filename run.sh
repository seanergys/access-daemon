#!/bin/bash

export LD_LIBRARY_PATH=./src/common

PLUGINS="keyboard.so"
PLUGINS+="+query_handler.so"
PLUGINS+="+metrics_central.so"
PLUGINS+="+met_cpu_freq_msr.so"
PLUGINS+="+met_cpu_pow_msr.so"
PLUGINS+="+act_cpu_freq_driver_governor.so"
PLUGINS+="+act_cpu_freq_driver_pstate.so"
PLUGINS+="+act_cpu_pow_msr.so"

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
./src/accessd ${1} \
    --paths=./src/plugins/general:./src/plugins/metrics:./src/plugins/knobs_actuators \
    --plugins=${PLUGINS}