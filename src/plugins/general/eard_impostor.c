/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE
#include <pwd.h>
#include <poll.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <common/system/time.h>
#include <common/metrics_types.h>
#include <common/system/folder.c>
#include <metrics/energy/cpu/intel63.h>
#include <plugins/general/topology_ext.h>
#include <plugins/general/eard_impostor.h>

typedef struct eard_client_s {
    int req_fd;
    int ack_fd;
} eard_client_t;

static char             error_message[256];
static char             global_path[PATH_MAX];
static daemon_req_t     global_req;
static int              global_fd = -1;
static eard_client_t    clients[EARD_ADAPTER_MAX_CLIENTS];
static const char      *ear_tmp;
static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static topology_ext_t  *tp_ext;

plug_identity()
{
    *tag       = "eard_impostor";
    *tags_deps = "topology_ext";
}

static void client_close(eard_client_t *client, char *because)
{
    if (client->req_fd >= 0) {
        plugin_manager_poll_remove(client->req_fd);
        close(client->req_fd);
    }
    if (client->ack_fd >= 0) {
        close(client->ack_fd);
    }
    // Private FIFOs and application directories belong to eards_connect().
    *client = (eard_client_t) { .req_fd = -1, .ack_fd = -1 };
}

static void adapter_cleanup(void)
{
    int i;

    for (i = 0; i < EARD_ADAPTER_MAX_CLIENTS; ++i) {
        client_close(&clients[i], "cleanup");
    }
    if (global_fd >= 0) {
        plugin_manager_poll_remove(global_fd);
        close(global_fd);
    }
    if (global_path[0] && unlink(global_path) < 0 && errno != ENOENT) {
        // Error durung inlink
    }
    memset(global_path, 0, sizeof(global_path));
    global_fd = -1;
}

static int fifo_open(const char *path, int flags)
{
    struct stat st;
    int fd;

    if ((fd = open(path, flags | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW)) < 0) {
        return -1;
    }
    if (fstat(fd, &st) < 0 || !S_ISFIFO(st.st_mode)) {
        close(fd);
        return -1;
    }
    return fd;
}

plug_init(_eard_impostor)
{
    struct passwd *pwd;
    char *error = NULL;
    int n, i;

    for (i = 0; i < EARD_ADAPTER_MAX_CLIENTS; ++i) {
        clients[i] = (eard_client_t) { .req_fd = -1, .ack_fd = -1 };
    }
    if ((ear_tmp = getenv("EAR_TMP")) == NULL) {
        ear_tmp = "/tmp/accessd";
    }
    // Creating TMP folder plus anonymous apps (and using global path for a bit)
    if ((pwd = getpwuid(getuid())) == NULL) {
        return "[X] can not obtain user ID";
    }
    sprintf(global_path, "%s/0", ear_tmp);
    ear_create_tmp(global_path, pwd->pw_name);
    // Recovering true global path
    n = snprintf(global_path, sizeof(global_path), "%s/.ear_comm.globalreq", ear_tmp);
    if (n < 0 || (size_t) n >= sizeof(global_path)) {
        return "[X] EAR_TMP path is too long";
    }
    // Refuse an existing endpoint rather than replace a running EARD's FIFO.
    if (mkfifo(global_path, 0666) < 0) {
        return "[X] global FIFO already exists";
    }
    pthread_mutex_lock(&lock);
    if (chmod(global_path, 0666) < 0 || (global_fd = fifo_open(global_path, O_RDWR)) < 0) {
        error = "[X] global FIFO can not be opened";
    } else if (!plug_poll_add("eard_impostor", "request", global_fd)) {
        error = "[X] global FIFO can not be registered";
    }
    if (error != NULL) {
        adapter_cleanup();
    }
    pthread_mutex_unlock(&lock);
    return error;
}

plug_init(_topology_ext)
{
    tp_ext = (topology_ext_t *) data;
    return NULL;
}

static eard_client_t *client_find(int fd)
{
    int i;
    for (i = 0; i < EARD_ADAPTER_MAX_CLIENTS; ++i) {
        if (clients[i].req_fd == fd) return &clients[i];
    }
    return NULL;
}

static int client_fifo_open(const daemon_req_t *request, const char *channel, int flags)
{
    char path[PATH_MAX];
    const job_t *job = &request->req_data.app.job;
    int id = create_ID(job->id, job->step_id);
    int n;

    n = snprintf(path, sizeof(path), "%s/%u/%u/.ear_comm.%s_0.%d.%lu",
        ear_tmp, (uint) id, (uint) job->local_id, channel, id, request->con_id.lid);
    if (n < 0 || (size_t) n >= sizeof(path)) {
        return -1;
    }
    return fifo_open(path, flags);
}

static void client_connect(const daemon_req_t *request)
{
    eard_client_t *client;
    ulong ack = 0;

    if (request->req_service != CONNECT_EARD_NODE_SERVICES) {
        return;
    }
    if ((client = client_find(-1)) == NULL) {
        fprintf(stderr, "eard_impostor: no free connection slots\n");
        return;
    }
    /* Linux O_RDWR avoids waiting for the client's reader and prevents SIGPIPE
     * if it leaves during the handshake. The request FIFO tracks disconnects. */
    if ((client->req_fd = client_fifo_open(request, "req", O_RDONLY)) < 0 ||
        (client->ack_fd = client_fifo_open(request, "ack", O_RDWR  )) < 0 ||
        !plug_poll_add("eard_impostor", "request", client->req_fd)        ||
        write(client->ack_fd, &ack, sizeof(ack)) != sizeof(ack))
    {
        // TODO: turn req_fd and ack_fd NON_BLOCKING
        fprintf(stderr, "eard_impostor: could not connect client FIFOs\n");
        client_close(client, "opening FIFO");
    }
}

static int data_read(int fd, void *buffer, size_t expected_size)
{
    ssize_t count = 0;
    size_t total = 0;

    while (total < expected_size) {
        // Make it non-blocking and add a timeout
        count = read(fd, (char *) buffer + total, expected_size - total);
        if (count  < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        if (count  < 0 && (errno == EINTR)) continue;
        if (count <= 0) return -1;
        total += count;
    }
    return (total == expected_size);
}

static int data_write(int fd, char *data, size_t size)
{
    size_t  pending = size;
    size_t  total   = 0;
    ssize_t count   = 0;
    int     intents = 0;

    do {
        if ((count = write(fd, data + total, pending)) > 0) {
            total += count;
            pending -= count;
        } else if (count < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                // If the write was bad, but because of non-block, we give another chance.
                usleep(500);
            } else {
                return 0;
            }
        } else {
            return pending == 0; // EOF
        }
        intents++;
    } while (intents < 10 && pending > 0);
    return pending == 0;
}

static int cpu_freq_special_case(eard_head_t *head, char *buffer, metrics_request_t *req)
{
    static struct cpu_freq_s {
        uint64_t aperf;
        uint64_t mperf;
        uint32_t state;
    } *cpu_freq = NULL;
    timestamp_t time;
    int i;

    if (cpu_freq == NULL) {
        cpu_freq = calloc(tp_ext->tp.cpu_count, sizeof(struct cpu_freq_s));
    }
    timestamp_get(&time);
    head->state = EAR_SUCCESS;
    head->size  = sizeof(struct cpu_freq_s)*tp_ext->tp.cpu_count;
    memset(cpu_freq, 0, head->size); \
    for (i = 0; i < req->outputs[0].values_count; i++) {
        cpu_freq[i].aperf = req->outputs[0].values[i] / tp_ext->base_frequency;
        cpu_freq[i].mperf = (uint64_t) time.tv_sec;
    }
    memcpy(buffer, cpu_freq, head->size);
    return 1;
}

static int translate_rpc(eard_head_t *head, char *buffer)
{
    static serial_buffer_t serial = {0};
    static metrics_request_t req = {0};
    uint api = API_FAKE;
    const char *error;

    if (serial.data == NULL) {
        serial_alloc(&serial, 16384);
    }
    #define request(dom, met, dev, dev_no, cmd) \
        req.input.domain    = dom; \
        req.input.metric    = met; \
        req.input.device    = dev; \
        req.input.device_no = dev_no; \
        req.input.command   = cmd; \
        plug_message_send("received_request", (void *) &req);
    #define return_bufcpy(p, sz) { \
        memcpy(buffer, p, sz), head->size = sz, head->state = EAR_SUCCESS; \
        return 1; \
        }
    switch (head->req_service) {
        case DISCONNECT_EARD_NODE_SERVICES:
            return 0;
        case RPC_MET_CPUFREQ_GET_API:
            return_bufcpy(&api, sizeof(uint));
        case RPC_MET_CPUFREQ_GET_CURRENT:
            request(Met, AccFreq, Cpu, All, Get);
            return cpu_freq_special_case(head, buffer, &req);
        default:
            error = "EARD RPC is not implemented by eard_impostor";
            break;
    }
    head->state = EAR_ERROR;
    strcpy(buffer, error);
    return 1;
}

static int translate_old(eard_head_t *head, char *buffer)
{
    ulong *ack = (ulong *) buffer;

    head->state = EAR_SUCCESS;
    head->size = sizeof(*ack);
    *ack = 0;

    switch (head->req_service) {
        case DATA_SIZE_RAPL:
            *ack = sizeof(ullong) * RAPL_POWER_EVS * tp_ext->tp.socket_count;
            break;
        case READ_RAPL:
            head->size = sizeof(ullong) * RAPL_POWER_EVS * tp_ext->tp.socket_count;
            memset(buffer, 0, head->size);
            break;
        case ENERGY_FREQ:
            *ack = 10000000;
            break;
        case GPU_SUPPORTED:
        case WRITE_APP_SIGNATURE:
        case WRITE_WF_APP_SIGNATURE:
        case WRITE_LOOP_SIGNATURE:
        case WRITE_EVENT:
            break;
        default:
            return 0;
    }
    return 1;
}

static int adapter_error(const char *operation)
{
    snprintf(error_message, sizeof(error_message),
        "%s: %s", operation, strerror(errno));
    return 0;
}

static int client_handle(eard_client_t *client, short events)
{
    static size_t buffer_size = 16384;
    static char *buffer = NULL;
    eard_head_t *head = NULL;
    size_t remaining = 0;

    if (buffer == NULL) {
        if ((buffer = calloc(buffer_size, sizeof(char))) == NULL) {
            return adapter_error("[X] allocation failed");
        }
    }
    if ((events & (POLLHUP | POLLERR | POLLNVAL)) || !(events & POLLIN)) {
        return adapter_error("POLLHUP");
    }
    head = (eard_head_t *) buffer;
    if (!data_read(client->req_fd, head, sizeof(eard_head_t))) {
        return adapter_error("request read failed or EOF");
    }
    /* Legacy calls include daemon_req_opt; RPCs declare their body
     * size. Consume it even when the service is unsupported. */
    remaining = head->req_service > NEW_API_SERVICES
              ? head->size : sizeof(union daemon_req_opt);
    // If greather than 1MiB, something weird is happening
    if (remaining > 1048576) {
        return adapter_error("absurd incoming size");
    }
    // If the remaining size is greater than data buffer (and hader), realloc
    if (buffer_size < (remaining+sizeof(eard_head_t))) {
        if (realloc(buffer, remaining+sizeof(eard_head_t)) == NULL) {
            // TODO: return error
        }
        buffer_size = remaining+sizeof(eard_head_t);
    }
    // Reading the remaining data
    if (!data_read(client->req_fd, buffer+sizeof(eard_head_t), remaining)) {
        return adapter_error("request read failed or EOF");
    }
    // Old services
    if (head->req_service < NEW_API_SERVICES) {
        if (!translate_old(head, &buffer[sizeof(eard_head_t)])) {
            return adapter_error("legacy service is not implemented");
        }
        return data_write(client->ack_fd, &buffer[sizeof(eard_head_t)], head->size);
    }
    // New services
    if (!translate_rpc(head, &buffer[sizeof(eard_head_t)])) {
        return adapter_error("disconnected or translation error");
    }
    return data_write(client->ack_fd, buffer, sizeof(eard_head_t)+head->size);
}

plug_poll_attend(_request)
{
    eard_client_t *client;

    pthread_mutex_lock(&lock);
    // TODO: return errors message
    if (global_fd >= 0) {
        // If is the global descriptor which is receiving data
        if (pfd->fd == global_fd && (pfd->revents & POLLIN)) {
            // Read the content
            if (data_read(global_fd, &global_req, sizeof(global_req))) {
                client_connect(&global_req);
            }
        } else if ((client = client_find(pfd->fd)) != NULL) {
            if (!client_handle(client, pfd->revents)) {
                client_close(client, error_message);
            }
        }
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

plug_exit()
{
    pthread_mutex_lock(&lock);
    if (global_fd >= 0) {
        adapter_cleanup();
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}
