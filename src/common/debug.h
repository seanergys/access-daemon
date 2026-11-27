/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ACCESS_DAEMON_DEBUG_H
#define ACCESS_DAEMON_DEBUG_H

extern int debug_enabled;

#define debug(...)                                                       \
    do {                                                                 \
        if (debug_enabled) {                                             \
            dprintf(2, "%s:%s:%d: ", __FILE__, __FUNCTION__, __LINE__);  \
            dprintf(2, __VA_ARGS__);                                     \
            dprintf(2, "\n");                                            \
    }} while (0);

#endif //ACCESS_DAEMON_DEBUG_H
