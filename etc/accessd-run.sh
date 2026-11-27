#!/bin/bash

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: Apache-2.0

SCRIPT_PATH=$(realpath -e -- "${BASH_SOURCE[0]}") || exit 1
SBIN_DIR=${SCRIPT_PATH%/*}
LIB_DIR=${SBIN_DIR%/*}/lib/accessd
PLUGIN_DIR=${LIB_DIR}/plugins

export LD_LIBRARY_PATH="${LIB_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

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
exec "${SBIN_DIR}/accessd" "$@" \
    --paths="${PLUGIN_DIR}:${PLUGIN_DIR}/metrics:${PLUGIN_DIR}/actuators" \
    --plugins="${PLUGINS}"
