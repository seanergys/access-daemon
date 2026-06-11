#ifndef SEANERBUS_C_H
#define SEANERBUS_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SBUS_PAYLOAD_NONE 0ULL
#define SBUS_PAYLOAD_HELLO_WORLD_REQ_V1 3ULL
#define SBUS_PAYLOAD_HELLO_WORLD_RES_V1 4ULL

typedef struct sbus_request {
    uint64_t payload_type;
    const void *payload_data;
    size_t payload_size;

    /* Solo esta informado si payload_type == SBUS_PAYLOAD_HELLO_WORLD_REQ_V1. */
    const char *hello_world_msg;
} sbus_request;

typedef struct sbus_response {
    uint64_t payload_type;

    /* Usado si payload_type == SBUS_PAYLOAD_HELLO_WORLD_RES_V1. */
    const char *hello_world_msg;
} sbus_response;

typedef int (*sbus_request_callback)(void *user_data,
                                     const sbus_request *request,
                                     sbus_response *response);

int sbus_run_request_responder(const char *host,
                               const char *port,
                               const char *endpoint_uuid,
                               sbus_request_callback callback,
                               void *user_data);

const char *sbus_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
