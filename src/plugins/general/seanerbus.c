#include <stdlib.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <common/framework.h>
#include <seanerbus/c/seanerbus.h>
#include <plugins/general/seanerbus.h>

static char *address_str = "demobus://localhost:5398";
static seanerbus_connection_t *conn;
static seanerbus_address_t    *addr;
static pthread_t               thread;

plug_identity()
{
    *tag = "seanerbus";
    *tags_deps = NULL;
}

static void check(seanerbus_status_t s)
{
    if (s != SEANERBUS_STATUS_OK) {
        fprintf(stderr, "Error SeanerBUS: %d\n", (int) s);
        exit(1);
    }
}

static void request_handler(seanerbus_connection_t *c, seanerbus_status_t s,
    const seanerbus_message_t *request, const seanerbus_error_t *e, void *datos)
{
    check(s);
    exit(-1); // By now
}

static void *listener(void *p)
{
    seanerbus_error_t *err = NULL;
    int fd = -1;
    // 6) Associate each seanerbus_address_t into a request handler
    check(seanerbus_connection_register_endpoint(conn, addr, request_handler, NULL, &err));
    // 7) I supose this is the file descriptor of the SeanerBUS broker
    check(seanerbus_connection_get_fd(conn, &fd, &err));
    // 8) ¿simple_even_loop.run()?
    check(seanerbus_connection_handle(conn, &err));
    return NULL;
}

// Called after the initialization of all plugins
plug_init_post(_seanerbus)
{
    seanerbus_error_t *err = NULL;
    int m_errno = 0;
    // 1) Get a list of available metrics, device types an device numbers
    //   plug_message_send("get_all_available_metrics", &mets);
    // 2) Convert each metric and device into an UUID
    //   uuid_generate_random();
    // 3) Convert each UUID into a seanerbus_address_t
    check(seanerbus_address_create_from_string("7bc3d210-a49f-4422-8fe8-26ca480d0e1f", &addr, &err));
    // 4) Create a connection
    check(seanerbus_connection_create(address_str, address_str, &conn, &err));
    // 5) Create a thread listening the list of addresses
    if ((m_errno = pthread_create(&thread, NULL, listener, NULL)) != 0) {
        return "[X] Cannot create the listener thread";
    }
    return NULL;
}