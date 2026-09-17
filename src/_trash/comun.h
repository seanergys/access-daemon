#include <seanerbus/c/seanerbus.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static seanerbus_connection_t *conn;
static seanerbus_address_t *addr;
static int terminado;

static void comprobar(seanerbus_status_t s)
{
    if (s != SEANERBUS_STATUS_OK) {
        fprintf(stderr, "Error SeanerBUS: %d\n", (int)s);
        exit(1);
    }
}

static void iniciar(void)
{
    comprobar(seanerbus_address_create_from_string(
        "7bc3d210-a49f-4422-8fe8-26ca480d0e1f", &addr, NULL));
    comprobar(seanerbus_connection_create(
        "demobus://localhost:5398", "demobus://localhost:5398",
        &conn, NULL));
}

static seanerbus_message_t *mensaje(const char *texto)
{
    seanerbus_message_t *m;
    seanerbus_byte_view_t bytes = {texto, strlen(texto)};
    comprobar(seanerbus_message_create(
        addr, UINT64_C(1) << 63, bytes, 0, &m, NULL));
    return m;
}

static void imprimir(const seanerbus_message_t *m)
{
    seanerbus_byte_view_t bytes;
    comprobar(seanerbus_message_get_payload(m, &bytes, NULL));
    fwrite(bytes.data, 1, bytes.size, stdout);
    puts("");
}

static void ejecutar(void)
{
    struct pollfd p = {.events = POLLIN};
    comprobar(seanerbus_connection_get_fd(conn, &p.fd, NULL));

    while (!terminado) {
        if (poll(&p, 1, -1) < 0) {
            perror("poll");
            exit(1);
        }
        if (p.revents & (POLLERR | POLLHUP | POLLNVAL))
            exit(1);
        if (p.revents & POLLIN)
            comprobar(seanerbus_connection_handle(conn, NULL));
    }

    comprobar(seanerbus_connection_shutdown(conn, NULL));
    seanerbus_connection_destroy(conn);
    seanerbus_address_destroy(addr);
}