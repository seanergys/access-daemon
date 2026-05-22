#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <plugins/general/keyboard.h>

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
    plugin_manager_close();
}

static int is(char *s1, const char *s2)
{
    return strncasecmp(s1, s2, strlen(s2)) == 0;
}

static void *string_read(void *x)
{
    static char string[128];
    int i;

    sleep(1);
    while (!is_closing) {
        printf("Enter query: ");
        fflush(stdout);
        if ((i = scanf(" %127[^\n]", string)) > 0) {
            if (is(string, "exit")) {
                string_exit();
            } else {
                plug_message_send("keyboard", string);
            }
        }
    }
    // Waiting here until the parent dies
    while (is_closing);
    return NULL;
}

plug_action_init(_keyboard)
{
    pthread_create(&thread, NULL, string_read, NULL);
    return NULL;
}