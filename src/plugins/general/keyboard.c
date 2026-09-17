#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <plugins/general/keyboard.h>
#include <plugins/general/request_handler.h>

static pthread_t thread;
static int is_closing;

plug_identity()
{
    *tag       = "keyboard";
    *tags_deps = NULL;
}

static void string_exit()
{
    is_closing = 1;
    plugin_manager_exit();
}

static int is(char *s1, const char *s2)
{
    return strncasecmp(s1, s2, strlen(s2)) == 0;
}

static void *string_read(void *x)
{
    metrics_request_text_t req = {0};
    char string[1024];
    int i;

    sleep(1);
    while (!is_closing) {
        printf("Enter query: ");
        fflush(stdout);
        if ((i = scanf(" %1023[^\n]", string)) > 0) {
            if (is(string, "exit")) {
                string_exit();
            } else {
                req.input = string;
                plug_message_send("received_request_text", (void *) &req);
                printf("%s", req.output);
            }
        }
    }
    // Waiting here until the parent dies
    while (is_closing);
    return NULL;
}

plug_init(_keyboard)
{
    pthread_create(&thread, NULL, string_read, NULL);
    return NULL;
}