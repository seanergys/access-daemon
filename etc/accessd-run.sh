#!/bin/bash

export LD_LIBRARY_PATH=../lib/accessd:/opt/seanerbus/lib

PLUGINS="keyboard.so"
PLUGINS+=",seanerbus.so"
PLUGINS+=",request_handler.so"
PLUGINS+=",request_handler_text.so"
PLUGINS+=",metrics_accumulator.so:1000" #Optional
PLUGINS+=",metrics_caller.so"
PLUGINS+=",met.cpu_freq.msr.so"
PLUGINS+=",met.cpu_pow.msr.so"
PLUGINS+=",act.cpu_freq.driver_governor.so"
PLUGINS+=",act.cpu_freq.driver_pstate.so"
PLUGINS+=",act.cpu_pow.msr.so"

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
../sbin/accessd ${1} \
    --paths=../lib/accessd/plugins:../lib/accessd/plugins/metrics:../lib/accessd/plugins/actuators \
    --plugins=${PLUGINS}
