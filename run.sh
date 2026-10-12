#!/bin/bash

export LD_LIBRARY_PATH=./src/common

PLUGINS="keyboard.so"
PLUGINS+=",eard_impostor.so"
PLUGINS+=",request_handler.so"
PLUGINS+=",request_handler_text.so"
PLUGINS+=",metrics_accumulator.so:1000" #Not mandatory
PLUGINS+=",metrics_caller.so"
PLUGINS+=",met.cpu_freq.msr.so"
PLUGINS+=",met.cpu_pow.msr.so"
PLUGINS+=",act.cpu_freq.driver_governor.so"
PLUGINS+=",act.cpu_freq.driver_pstate.so"
PLUGINS+=",act.cpu_pow.msr.so"

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
./src/accessd ${1} \
    --paths=./src/plugins/general:./src/plugins/metrics:./src/plugins/knobs_actuators \
    --plugins=${PLUGINS}
