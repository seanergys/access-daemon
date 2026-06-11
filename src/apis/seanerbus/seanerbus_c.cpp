#include "seanerbus_c.h"

#include <seanerbus/client/connection.hpp>
#include <seanerbus/client/simple_event_loop.hpp>
#include <seanerbus/msg/hello_world_req_v1.hpp>
#include <seanerbus/msg/hello_world_res_v1.hpp>

#include <exception>
#include <string>

namespace {

thread_local std::string g_last_error;

void set_last_error(const std::string &msg)
{
    g_last_error = msg;
}

struct HandlerContext {
    sbus_request_callback callback;
    void *user_data;
};

void request_handler(seanerbus::client::Connection *conn,
                     seanerbus::Message msg,
                     HandlerContext *ctx)
{
    try {
        if (msg.type != ::Message::MessageType::REQUEST) {
            return;
        }

        std::string hello_world_msg;

        sbus_request request = {};
        request.payload_type = msg.payload.first;
        request.payload_data = msg.payload.second.empty() ? nullptr : msg.payload.second.data();
        request.payload_size = msg.payload.second.size();
        request.hello_world_msg = nullptr;

        if (request.payload_type == SBUS_PAYLOAD_HELLO_WORLD_REQ_V1) {
            seanerbus::msg::HelloWorldReqV1 decoded(msg.payload);
            hello_world_msg = decoded.str_msg;
            request.hello_world_msg = hello_world_msg.c_str();
        }

        sbus_response response = {};
        response.payload_type = SBUS_PAYLOAD_HELLO_WORLD_RES_V1;
        response.hello_world_msg = "OK";

        if (ctx->callback(ctx->user_data, &request, &response) != 0) {
            set_last_error("El callback C devolvio error");
            conn->shutdown();
            return;
        }

        if (response.payload_type == SBUS_PAYLOAD_NONE) {
            return;
        }

        if (response.payload_type == SBUS_PAYLOAD_HELLO_WORLD_RES_V1) {
            const char *text = response.hello_world_msg ? response.hello_world_msg : "";
            seanerbus::msg::HelloWorldResV1 encoded{ std::string{text} };
            conn->respond_to(msg, encoded.to_capnp());
            return;
        }

        set_last_error("Tipo de respuesta no soportado por este wrapper C minimo");
        conn->shutdown();
    } catch (const std::exception &e) {
        set_last_error(e.what());
        conn->shutdown();
    } catch (...) {
        set_last_error("Excepcion C++ desconocida");
        conn->shutdown();
    }
}

} // namespace

extern "C" int sbus_run_request_responder(const char *host,
                                           const char *port,
                                           const char *endpoint_uuid,
                                           sbus_request_callback callback,
                                           void *user_data)
{
    g_last_error.clear();

    if (host == nullptr || port == nullptr || endpoint_uuid == nullptr || callback == nullptr) {
        set_last_error("Argumento nulo en sbus_run_request_responder");
        return -1;
    }

    try {
        HandlerContext ctx = { callback, user_data };

        seanerbus::client::Connection conn(host, port);
        seanerbus::Address addr{ seanerbus::Uuid{ endpoint_uuid } };

        conn.register_endpoint(addr, request_handler, &ctx);

        seanerbus::client::SimpleEventLoop loop(conn);
        loop.run();

        return g_last_error.empty() ? 0 : -1;
    } catch (const std::exception &e) {
        set_last_error(e.what());
        return -1;
    } catch (...) {
        set_last_error("Excepcion C++ desconocida");
        return -1;
    }
}

extern "C" const char *sbus_last_error(void)
{
    return g_last_error.c_str();
}
