#ifndef SEANERBUS_WRAPPER_H
#define SEANERBUS_WRAPPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AD_SBUS_PAYLOAD_NONE                0ULL
#define AD_SBUS_PAYLOAD_HELLO_WORLD_REQ_V1  3ULL
#define AD_SBUS_PAYLOAD_HELLO_WORLD_RES_V1  4ULL
#define AD_SBUS_PAYLOAD_USER_DEFINED_BASE  (1ULL << 63)

typedef struct ad_sbus_request {
    uint64_t    payload_type;
    const void *payload_data;
    size_t      payload_size;
    const char *hello_world_msg;
} ad_sbus_request_t;

typedef struct ad_sbus_response {
    uint64_t    payload_type;
    const void *payload_data;
    size_t      payload_size;
    const char *hello_world_msg;
} ad_sbus_response_t;

typedef int (*ad_sbus_request_callback_t)
    (void *user_data, const ad_sbus_request_t *request, ad_sbus_response_t *response);

typedef struct ad_sbus_responder ad_sbus_responder_t;

int ad_sbus_responder_start(ad_sbus_responder_t **responder,
                            const char *host,
                            const char *port,
                            const char *endpoint_uuid,
                            ad_sbus_request_callback_t callback,
                            void *user_data);

void ad_sbus_responder_stop(ad_sbus_responder_t *responder);

const char *ad_sbus_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
