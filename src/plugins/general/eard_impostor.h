#ifndef EARD_ADAPTER_H
#define EARD_ADAPTER_H

#include <common/framework.h>
#include <daemon/local_api/eard_api.h>

/* The local API uses native EAR structs: client and adapter must share the
 * same EAR build configuration (including MAX_CPUS_SUPPORTED and USE_GPUS).
 * This base implements connection/disconnection only, not EARD services. */
#define EARD_ADAPTER_MAX_CLIENTS 64

typedef struct daemon_req daemon_req_t;

#endif // EARD_ADAPTER_H
