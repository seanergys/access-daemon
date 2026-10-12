#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <plugins/general/keyboard.h>
#include <plugins/general/request_handler.h>
#include <plugins/general/request_handler_text.h>

static atomic_int thread_finished;
static int        thread_started;
static pthread_t  thread;

plug_identity()
{
    *tag       = "keyboard";
    *tags_deps = NULL;
}

static void string_exit()
{
    plugin_manager_exit();
}

static int is(char *s1, const char *s2)
{
    return strcasecmp(s1, s2) == 0;
}

static void *string_read(void *x)
{
    metrics_request_text_t req = {0};
    char string[1024];
    int i;

    while (1) {
        printf("Enter query: ");
        fflush(stdout);
        if ((i = scanf(" %1023[^\n]", string)) > 0) {
            if (is(string, "exit") || is(string, "quit") || is(string, "q")) {
                break;
            }
            req.input = string;
            plug_message_send("received_request_text", (void *) &req);
            // Printing the output which is already converted into string
            printf("%s", req.output);
        } else if (i == EOF) {
            break;
        }
    }
    thread_finished = 1;
    string_exit();
    return NULL;
}

plug_init_post(_keyboard)
{
    int rc;
    if ((rc = pthread_create(&thread, NULL, string_read, NULL)) != 0) {
        return "[D] Failed to create keyboard thread";
    }
    thread_started = 1;
    return NULL;
}

plug_exit()
{
    int rc;
    if (!thread_started) {
        return NULL;
    }
    // Wake a reader blocked on stdin when shutdown comes from another thread.
    if (!thread_finished && (rc = pthread_cancel(thread)) != 0) {
        return "Failed to cancel keyboard thread";
    }
    if ((rc = pthread_join(thread, NULL)) != 0) {
        return "Failed to join keyboard thread";
    }
    thread_started = 0;
    return NULL;
}
