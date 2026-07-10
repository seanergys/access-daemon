#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plugins/general/query_handler.h>

plug_identity()
{
    *tag       = "query_handler";
    *tags_deps = NULL;
}

plug_init(_query_handler)
{
    // Configuration (plugins,periodicity)
    return NULL;
}

plug_periodic(_query_handler)
{
    return NULL;
}

char *wrdcpy(const char *string, char *dst)
{
    const char *start = NULL;
    const char *end = NULL;
    size_t length = 0;

    if (string == NULL || dst == NULL) {
        return 0;
    }
    dst[0] = '\0';
    // Skipping initial spaces
    while (*string != '\0' && isspace((unsigned char)*string)) {
        string++;
    }
    if (*string == '\0') {
        return NULL;
    }
    // Found word
    start = string;
    // Avanzar hasta el final de la palabra
    while (*string != '\0' && !isspace((unsigned char)*string)) {
        string++;
    }
    end = string;
    length = end - start;
    memcpy(dst, start, length);
    dst[length] = '\0';
    return (char *) string;
}

static int isnum(const char *string)
{
    if (!string) {
        return 0;
    }
    while (isspace((unsigned char) *string)) {
        string++;
    }
    if (*string == '\0') {
        return 0;
    }
    while (*string && !isspace((unsigned char) *string)) {
        if (!isdigit((unsigned char) *string)) {
            return 0;
        }
        string++;
    }
    return 1;
}

static void answer(answer_t *ans)
{
    if (ans == NULL) {
        return;
    }
    if (ans->invalid) {
        fprintf(stderr, "ERROR: %s\n", ans->error_msg != NULL ? ans->error_msg : "invalid query");
    } else if (ans->bytes != NULL && ans->size > 0) {
        printf("%.*s", (int) ans->size, (char *) ans->bytes);
        if (((char *) ans->bytes)[ans->size - 1] != '\n') {
            printf("\n");
        }
    }
    free(ans->bytes);
    free(ans->error_msg);
    memset(ans, 0, sizeof(*ans));
}

plug_message_receive(_keyboard)
{
    const char *string = (char *) data;
    query_t query = {0};

    if (!(string = wrdcpy(string, query.domain))) return NULL;
    if (!(string = wrdcpy(string, query.metric))) return NULL;
    if (!(string = wrdcpy(string, query.command))) return NULL;
          string = wrdcpy(string, query.device);
    if (!isnum(string)) string = wrdcpy(string, query.sub_device);
    query.arguments = (char *) string;
    plug_message_send("query_metrics", &query);
    answer(&query.answer);
    return NULL;
}
