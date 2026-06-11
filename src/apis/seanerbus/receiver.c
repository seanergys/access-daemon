#include "seanerbus_c.h"

#include <inttypes.h>
#include <stdio.h>

static int on_request(void *user_data,
                      const sbus_request *request,
                      sbus_response *response)
{
    (void)user_data;

    printf("Request recibida: payload_type=%" PRIu64 ", payload_size=%zu\n",
           request->payload_type,
           request->payload_size);

    if (request->payload_type == SBUS_PAYLOAD_HELLO_WORLD_REQ_V1) {
        printf("HelloWorldReqV1.msg = %s\n", request->hello_world_msg);

        response->payload_type = SBUS_PAYLOAD_HELLO_WORLD_RES_V1;
        response->hello_world_msg = "Respuesta enviada desde C";
        return 0;
    }

    fprintf(stderr, "payload_type no soportado por este ejemplo minimo\n");

    /* Ojo: si no respondes a una request, el cliente puede quedarse esperando. */
    response->payload_type = SBUS_PAYLOAD_NONE;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s UUID_ENDPOINT\n", argv[0]);
        return 2;
    }

    if (sbus_run_request_responder("localhost", "5398", argv[1], on_request, NULL) != 0) {
        fprintf(stderr, "SEANERBUS error: %s\n", sbus_last_error());
        return 1;
    }

    return 0;
}
