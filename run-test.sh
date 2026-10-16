#!/usr/bin/env bash

set -euo pipefail # Fail on error, fail on undefined var,

export LD_LIBRARY_PATH="./src/common${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" # Expand

PLUGINS+=",keyboard.so"
PLUGINS+=",request_handler.so"
PLUGINS+=",request_handler_text.so"
PLUGINS+=",metrics_accumulator.so:1000" #Optional
PLUGINS+=",metrics_caller.so"
PLUGINS+=",met.gpu.nvml.so"
PLUGINS+=",met.cpu_freq.msr.so"
PLUGINS+=",met.cpu_pow.msr.so"
PLUGINS+=",act.cpu_pow.msr.so"

# Converting ${EXEC_PREFIX} variable into a command
PREFIX=()
if [[ -n "${EXEC_PREFIX:-}" ]]; then
    read -r -a PREFIX <<< "${EXEC_PREFIX}"
fi

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
exec "${PREFIX[@]}" ./src/accessd "$@" \
    --paths=./src/plugins/general:./src/plugins/metrics:./src/plugins/knobs_actuators \
        --plugins="${PLUGINS}"
