#include "comun.h"

static void respuesta(seanerbus_connection_t *c, seanerbus_status_t s,
                      const seanerbus_message_t *m,
                      const seanerbus_error_t *e, void *datos)
{
    comprobar(s);
    imprimir(m);
    terminado = 1;
}

int main(void)
{
    iniciar();
    seanerbus_message_t *m = mensaje("Hola");
    comprobar(seanerbus_connection_request(
        conn, m, respuesta, NULL, NULL));
    seanerbus_message_destroy(m);
    ejecutar();
}