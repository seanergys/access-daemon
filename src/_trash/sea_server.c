#include "comun.h"

static void enviada(seanerbus_connection_t *c, seanerbus_status_t s,
                    const seanerbus_error_t *e, void *datos)
{
    comprobar(s);
}

static void solicitud(seanerbus_connection_t *c, seanerbus_status_t s,
                      const seanerbus_message_t *request,
                      const seanerbus_error_t *e, void *datos)
{
    comprobar(s);
    imprimir(request);

    seanerbus_message_t *res = mensaje("Hola, recibido");
    comprobar(seanerbus_connection_respond_to(
        c, request, res, enviada, NULL, NULL));
    seanerbus_message_destroy(res);
}

int main(void)
{
    iniciar();
    comprobar(seanerbus_connection_register_endpoint(
        conn, addr, solicitud, NULL, NULL));
    ejecutar();
}