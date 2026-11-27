/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PLUG_TOPOLOGY_H
#define PLUG_TOPOLOGY_H

#include <common/framework.h>
#include <common/hardware/topology.h>

typedef struct topology_ext_s {
    topology_t tp; // I would like this as anonimous member
    uint64_t base_frequency; // In KHz
    uint8_t boost_enabled;
} topology_ext_t;

#endif // PLUG_TOPOLOGY_H