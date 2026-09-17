// C includes
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <plugins/general/data_plane.h>
// C++ includes
#include <atomic>
#include <thread>
#include <seanerbus/client/connection.hpp>
#include <seanerbus/client/simple_event_loop.hpp>
#include <seanerbus/msg/hello_world_req_v1.hpp>
#include <seanerbus/msg/hello_world_res_v1.hpp>

#define SEA_DEFAULT_HOST   "localhost"
#define SEA_DEFAULT_PORT   "5398"
#define SEA_DEFAULT_UUID   "a366335b-52f0-4658-a42b-7e73e83ec91f"

static std::atomic_int thread_unblock;
static char            thread_buffer[256];
static char           *thread_message = NULL;

static const char *envordef(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return (value != NULL && value[0] != '\0')? value: fallback;
}

/* - Types found in msg.capnp.h
   - MessageType (uint16_t): SUBSCRIBE, PUBLISH, ERROR, REQUEST, REGISTER, RESPONSE

| MessageTypes (class)      | Payload (*FLUX not included)
|---------------------------|--------------------------
| TIME_VALUE_V1             | TimeValueV1 { time: UInt64, value: Float64 }
| RESOLVE_REQ_V1            | ResolveReqV1 { attributes: List(KV) }, con KV { key: Text, value: Text }
| RESOLVE_RES_V1            | ResolveResV1 { matchingInterfaces: List(Address) }
| HELLO_WORLD_REQ_V1        | HelloWorldReqV1 { msg: Text }
| HELLO_WORLD_RES_V1        | HelloWorldResV1 { msg: Text }
| VECTOR_REQ_V1             | VectorReqV1 { values: List(Float64) }
| VECTOR_RES_V1             | VectorResV1 { results: List(Float64) }
| REVERSE_RESOLVE_REQ_V1    | ReverseResolveReqV1 { address: Address }
| REVERSE_RESOLVE_RES_V1    | ReverseResolveResV1 { name: List(KV) }
| REGISTER_RESOLVE_REQ_V1   | RegisterResolveReqV1 { name: List(KV), address: Address }
| REGISTER_RESOLVE_RES_V1   | RegisterResolveResV1 { success: Bool, errMsg: Text }
| UNREGISTER_RESOLVE_REQ_V1 | UnregisterResolveReqV1 { address: Address }
| UNREGISTER_RESOLVE_RES_V1 | UnregisterResolveResV1 { success: Bool, errMsg: Text }
| HISTORY_REQ_V1            | HistoryReqV1 { address, startTime: Int64, endTime: Int64, intervalMax: Int64, type: HistoryType }
| HISTORY_RES_V1            | HistoryResV1 { okOrErr: union { ok: History, err: Text } }
*/
static void request_handler(seanerbus::client::Connection& conn, seanerbus::Message msg)
{
    metrics_request_text_t req_text = {0};

    // Redefining types
    #define DP_TYPE_TEXT MessageTypes::HELLO_WORLD_REQ_V1
    #define DpTextReq    seanerbus::msg::HelloWorldReqV1
    #define DpTextRes    seanerbus::msg::HelloWorldResV1

    if (msg.type != ::Message::MessageType::REQUEST) {
        throw std::runtime_error("Got something different a request");
    }
    if (msg.payload.first != (uint64_t) DP_TYPE_TEXT) {
        throw std::runtime_error("Got something else than a DP_TYPE_TEXT");
    }
    //
    DpTextReq req(msg.payload);
    req_text.input = (char *) req.str_msg.c_str();
    plug_message_send("request_text", (void *) &req_text);
    DpTextRes res(req_text.output);
    conn.respond_to(msg, res.to_capnp());
}

extern "C"
{
plug_identity() {
    *tag = "data_plane";
    *tags_deps = NULL;
}

plug_init(_data_plane)
{
    const char *uuid = envordef("SEA_UUID", SEA_DEFAULT_UUID);
    const char *host = envordef("SEA_HOST", SEA_DEFAULT_HOST);
    const char *port = envordef("SEA_PORT", SEA_DEFAULT_PORT);

    thread_unblock = 0;
    // Initializing thread that will be dettached from the main thread
    std::thread([uuid, host, port]() {
        try {
            seanerbus::Address address{ seanerbus::Uuid{uuid}};
            seanerbus::client::Connection conn(host, port);
            conn.register_endpoint(address, request_handler);
            // TODO: ask the Seanerbus for the list of plugins to load
            seanerbus::client::SimpleEventLoop sel(conn);
            thread_unblock = 1;
            /* Chain:
                -> sel.run()
                 -> connection_.try_read()
                  -> handle(msg)
                   -> req_cbs_.at(msg.address)(*this, msg)
                    -> res_handler(...)
                     -> throw std::runtime_error(...) */
            sel.run();
        } catch (const std::exception& e) {
            snprintf(thread_buffer, sizeof(thread_buffer), "%s", e.what());
        } catch (...) {
            snprintf(thread_buffer, sizeof(thread_buffer), "Unknown exception");
        }
        // Dying thread
        thread_message = thread_buffer;
        thread_unblock = 1;
    }).detach();
    while (!thread_unblock);
    return thread_message;
}
}