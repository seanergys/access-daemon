#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <common/seanerbus_wrapper.h>
#include <plugins/general/data_plane.h>
#include <plugins/general/query_handler.h>

#define DATA_PLANE_DEFAULT_HOST   "localhost"
#define DATA_PLANE_DEFAULT_PORT   "5398"
#define DATA_PLANE_DEFAULT_UUID   "a366335b-52f0-4658-a42b-7e73e83ec91f"
#define DATA_PLANE_TEXT_MAX       8192

typedef struct data_plane_state_s {
    ad_sbus_responder_t *responder;
    char request_text[DATA_PLANE_TEXT_MAX];
    char response_text[DATA_PLANE_TEXT_MAX];
} data_plane_state_t;

static data_plane_state_t state_data;

plug_identity()
{
    *tag       = "data_plane";
    *tags_deps = NULL;
}

static const char *env_or_default(const char *name, const char *fallback)
{
    const char *value = getenv(name);

    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static char *word_copy(const char *string, char *dst, size_t dst_size)
{
    const char *start;
    size_t length;

    if (string == NULL || dst == NULL || dst_size == 0) {
        return NULL;
    }
    dst[0] = '\0';
    while (*string != '\0' && isspace((unsigned char) *string)) {
        string++;
    }
    if (*string == '\0') {
        return NULL;
    }
    start = string;
    while (*string != '\0' && !isspace((unsigned char) *string)) {
        string++;
    }
    length = (size_t) (string - start);
    if (length >= dst_size) {
        length = dst_size - 1;
    }
    memcpy(dst, start, length);
    dst[length] = '\0';
    return (char *) string;
}

static int isnum_word(const char *string)
{
    if (string == NULL) {
        return 0;
    }
    while (isspace((unsigned char) *string)) {
        string++;
    }
    if (*string == '\0') {
        return 0;
    }
    while (*string != '\0' && !isspace((unsigned char) *string)) {
        if (!isdigit((unsigned char) *string)) {
            return 0;
        }
        string++;
    }
    return 1;
}

static int parse_query_text(const char *text, query_t *query, char *error, size_t error_size)
{
    const char *cursor = text;

    memset(query, 0, sizeof(*query));
    if ((cursor = word_copy(cursor, query->domain, sizeof(query->domain))) == NULL ||
        (cursor = word_copy(cursor, query->metric, sizeof(query->metric))) == NULL ||
        (cursor = word_copy(cursor, query->command, sizeof(query->command))) == NULL ||
        (cursor = word_copy(cursor, query->device, sizeof(query->device))) == NULL) {
        snprintf(error, error_size,
                 "Invalid query. Expected: <domain> <metric> <command> <device> [sub-device] [arguments]");
        return -1;
    }
    if (!isnum_word(cursor)) {
        cursor = word_copy(cursor, query->sub_device, sizeof(query->sub_device));
    }
    query->arguments = (char *) cursor;
    return 0;
}

static void free_answer(answer_t *answer)
{
    free(answer->bytes);
    free(answer->error_msg);
    memset(answer, 0, sizeof(*answer));
}

static void set_response_text(data_plane_state_t *state, const char *text)
{
    snprintf(state->response_text, sizeof(state->response_text), "%s", text != NULL ? text : "");
}

static int execute_query(data_plane_state_t *state, const char *text)
{
    query_t query;
    char error[256];

    if (parse_query_text(text, &query, error, sizeof(error)) != 0) {
        set_response_text(state, error);
        return -1;
    }

    plug_message_send("query_metrics", &query);

    if (query.answer.invalid) {
        snprintf(state->response_text, sizeof(state->response_text), "ERROR: %s",
                 query.answer.error_msg != NULL ? query.answer.error_msg : "invalid query");
        free_answer(&query.answer);
        return -1;
    }
    if (query.answer.bytes != NULL) {
        snprintf(state->response_text, sizeof(state->response_text), "%.*s",
                 (int) query.answer.size, (char *) query.answer.bytes);
        free_answer(&query.answer);
        return 0;
    }

    set_response_text(state, "OK");
    free_answer(&query.answer);
    return 0;
}

static const char *request_text_from_payload(data_plane_state_t *state,
                                             const ad_sbus_request_t *request)
{
    size_t copy_size;

    if (request->payload_type == AD_SBUS_PAYLOAD_HELLO_WORLD_REQ_V1) {
        return request->hello_world_msg;
    }
    if (request->payload_type >= AD_SBUS_PAYLOAD_USER_DEFINED_BASE &&
        request->payload_data != NULL && request->payload_size > 0) {
        copy_size = request->payload_size;
        if (copy_size >= sizeof(state->request_text)) {
            copy_size = sizeof(state->request_text) - 1;
        }
        memcpy(state->request_text, request->payload_data, copy_size);
        state->request_text[copy_size] = '\0';
        return state->request_text;
    }
    return NULL;
}

static int on_data_plane_request(void *user_data,
                                 const ad_sbus_request_t *request,
                                 ad_sbus_response_t *response)
{
    data_plane_state_t *state = user_data;
    const char *text = request_text_from_payload(state, request);

    if (text == NULL || text[0] == '\0') {
        snprintf(state->response_text, sizeof(state->response_text),
                 "Unsupported Data-Plane request payload_type=%" PRIu64, request->payload_type);
    } else {
        execute_query(state, text);
    }

    if (request->payload_type >= AD_SBUS_PAYLOAD_USER_DEFINED_BASE) {
        response->payload_type = request->payload_type;
        response->payload_data = state->response_text;
        response->payload_size = strlen(state->response_text);
    } else {
        response->payload_type = AD_SBUS_PAYLOAD_HELLO_WORLD_RES_V1;
        response->hello_world_msg = state->response_text;
    }
    return 0;
}

plug_init(_data_plane)
{
    char **conf = data;
    const char *endpoint_uuid = env_or_default("AD_DATA_PLANE_UUID", DATA_PLANE_DEFAULT_UUID);
    const char *host = env_or_default("AD_DATA_PLANE_HOST", DATA_PLANE_DEFAULT_HOST);
    const char *port = env_or_default("AD_DATA_PLANE_PORT", DATA_PLANE_DEFAULT_PORT);
    static char message[512];

    if (conf != NULL && conf[0] != NULL && conf[0][0] != '\0') {
        endpoint_uuid = conf[0];
    }
    if (conf != NULL && conf[1] != NULL && conf[1][0] != '\0') {
        host = conf[1];
    }
    if (conf != NULL && conf[2] != NULL && conf[2][0] != '\0') {
        port = conf[2];
    }
    memset(&state_data, 0, sizeof(state_data));

    if (ad_sbus_responder_start(&state_data.responder, host, port, endpoint_uuid,
                                on_data_plane_request, &state_data) != 0) {
        snprintf(message, sizeof(message), "[D] data_plane disabled: %s", ad_sbus_last_error());
        return message;
    }

    snprintf(message, sizeof(message), "data_plane listening on %s:%s endpoint %s",
             host, port, endpoint_uuid);
    return message;
}

plug_periodic(_data_plane)
{
    return NULL;
}

void up_action_close()
{
    ad_sbus_responder_stop(state_data.responder);
    state_data.responder = NULL;
}
