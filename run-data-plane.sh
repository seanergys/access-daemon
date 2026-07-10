#!/bin/bash

export LD_LIBRARY_PATH=./src/common

PLUGINS="data_plane.so"

# The missing plugins in the argument can be found in the dependency list
# present in the code of each plugin.
./src/accessd ${1} \
    --paths=./src/plugins/general:./src/plugins/metrics:./src/plugins/knobs_actuators \
    --plugins=${PLUGINS}
