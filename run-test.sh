#!/usr/bin/env bash

set -euo pipefail # Fail on error, fail on undefined var,

export LD_LIBRARY_PATH="./src/common${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" # Expand

PLUGINS="keyboard.so"
PLUGINS+=",eard_impostor.so"
PLUGINS+=",request_handler.so"
PLUGINS+=",request_handler_text.so"
PLUGINS+=",metrics_accumulator.so:1000" # Not mandatory
PLUGINS+=",metrics_caller.so"
PLUGINS+=",met.cpu_freq.fake.so"

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
