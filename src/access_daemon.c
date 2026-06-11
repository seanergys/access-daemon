/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

// #define SHOW_DEBUGS 1
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <common/system/plugin_manager.h>

static void handle_signal(int sig, siginfo_t *si, void *unused)
{
    (void) si;     // Suppress unused parameter warning
    (void) unused; // Suppress unused parameter warning
    plugin_manager_exit();
}

int main(int argc, char *argv[])
{
    struct sigaction sa = {0};
    struct rlimit rl = {0};

    // Signal handling
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handle_signal;
    sa.sa_flags     = SA_SIGINFO;
    if (sigaction(SIGINT , &sa, NULL) == -1) { 
        error("SIGINT"); return 1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        error("SIGTERM"); return 1;
    }
    // Open file limits
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rl);
    // Framework control passing
    plugin_manager_main(argc, argv);
    plugin_manager_wait();
    return 0;
}