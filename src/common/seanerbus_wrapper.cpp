#include "seanerbus_wrapper.h"

#include <mutex>
#include <atomic>
#include <cerrno>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <exception>
#include <seanerbus/client/connection.hpp>
#include <seanerbus/internal/message.hpp>
#include <seanerbus/msg/hello_world_req_v1.hpp>
#include <seanerbus/msg/hello_world_res_v1.hpp>

extern "C" {
#include <poll.h>
}

namespace {

std::mutex g_last_error_lock;
std::string g_last_error;

void set_last_error(const std::string &message)
{
    std::lock_guard<std::mutex> guard(g_last_error_lock);
    g_last_error = message;
}

std::pair<uint64_t, std::vector<std::byte>> raw_payload(const ad_sbus_response_t &response)
{
    std::vector<std::byte> bytes;

    if (response.payload_data != nullptr && response.payload_size > 0) {
        const auto *begin = static_cast<const std::byte *>(response.payload_data);
        bytes.assign(begin, begin + response.payload_size);
    }
    return { response.payload_type, std::move(bytes) };
}

} // namespace

struct ad_sbus_responder {
    ad_sbus_request_callback_t callback = nullptr;
    void *user_data = nullptr;
    std::unique_ptr<seanerbus::client::Connection> connection;
    std::thread worker;
    std::atomic_bool stop_requested = false;

    ad_sbus_responder(const char *host,
                      const char *port,
                      const char *endpoint_uuid,
                      ad_sbus_request_callback_t callback_in,
                      void *user_data_in)
        : callback(callback_in), user_data(user_data_in)
    {
        connection = std::make_unique<seanerbus::client::Connection>(host, port);
        seanerbus::Address address{ seanerbus::Uuid{ endpoint_uuid } };
        connection->register_endpoint(address, request_handler, this);
        worker = std::thread(&ad_sbus_responder::run, this);
    }

    ~ad_sbus_responder()
    {
        stop();
    }

    void stop()
    {
        stop_requested.store(true);
        if (connection != nullptr) {
            connection->shutdown();
        }
        if (worker.joinable()) {
            worker.join();
        }
    }

    void run()
    {
        try {
            struct pollfd pfd = {};
            pfd.fd = connection->get_fd();
            pfd.events = POLLIN;

            while (!stop_requested.load() && !connection->is_shutdown()) {
                pfd.revents = 0;
                int ret = poll(&pfd, 1, 200);

                if (ret == 0) {
                    continue;
                }
                if (ret < 0) {
                    set_last_error(std::string("poll() fallo: ") + std::strerror(errno));
                    connection->shutdown();
                    return;
                }
                if (pfd.revents & POLLIN) {
                    connection->try_read();
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    set_last_error("La conexion con SeanerBUS se cerro");
                    connection->shutdown();
                    return;
                }
            }
        } catch (const std::exception &e) {
            set_last_error(e.what());
            if (connection != nullptr) {
                connection->shutdown();
            }
        } catch (...) {
            set_last_error("Excepcion C++ desconocida en el responder SeanerBUS");
            if (connection != nullptr) {
                connection->shutdown();
            }
        }
    }

    static void request_handler(seanerbus::client::Connection &conn,
                                seanerbus::Message message,
                                ad_sbus_responder *self)
    {
        try {
            if (message.type != ::Message::MessageType::REQUEST) {
                return;
            }

            std::string hello_world_msg;

            ad_sbus_request_t request = {};
            request.payload_type = message.payload.first;
            request.payload_data = message.payload.second.empty() ? nullptr : message.payload.second.data();
            request.payload_size = message.payload.second.size();

            if (request.payload_type == AD_SBUS_PAYLOAD_HELLO_WORLD_REQ_V1) {
                seanerbus::msg::HelloWorldReqV1 decoded(message.payload);
                hello_world_msg = decoded.str_msg;
                request.hello_world_msg = hello_world_msg.c_str();
            }

            ad_sbus_response_t response = {};
            response.payload_type = AD_SBUS_PAYLOAD_HELLO_WORLD_RES_V1;
            response.hello_world_msg = "";

            if (self->callback(self->user_data, &request, &response) != 0) {
                set_last_error("El callback C devolvio error");
                conn.shutdown();
                return;
            }

            if (response.payload_type == AD_SBUS_PAYLOAD_NONE) {
                return;
            }

            if (response.payload_type == AD_SBUS_PAYLOAD_HELLO_WORLD_RES_V1) {
                const char *text = response.hello_world_msg != nullptr ? response.hello_world_msg : "";
                seanerbus::msg::HelloWorldResV1 encoded{ std::string{ text } };
                conn.respond_to(message, encoded.to_capnp());
                return;
            }

            if (response.payload_type >= AD_SBUS_PAYLOAD_USER_DEFINED_BASE) {
                conn.respond_to(message, raw_payload(response));
                return;
            }

            set_last_error("Tipo de respuesta SeanerBUS no soportado por el wrapper");
            conn.shutdown();
        } catch (const std::exception &e) {
            set_last_error(e.what());
            conn.shutdown();
        } catch (...) {
            set_last_error("Excepcion C++ desconocida atendiendo una request SeanerBUS");
            conn.shutdown();
        }
    }
};

extern "C" int ad_sbus_responder_start(ad_sbus_responder_t **responder,
                                        const char *host,
                                        const char *port,
                                        const char *endpoint_uuid,
                                        ad_sbus_request_callback_t callback,
                                        void *user_data)
{
    if (responder == nullptr || host == nullptr || port == nullptr ||
        endpoint_uuid == nullptr || callback == nullptr) {
        set_last_error("Argumento nulo al arrancar el responder SeanerBUS");
        return -1;
    }

    *responder = nullptr;
    set_last_error("");

    try {
        *responder = new ad_sbus_responder(host, port, endpoint_uuid, callback, user_data);
        return 0;
    } catch (const std::exception &e) {
        set_last_error(e.what());
        return -1;
    } catch (...) {
        set_last_error("Excepcion C++ desconocida arrancando el responder SeanerBUS");
        return -1;
    }
}

extern "C" void ad_sbus_responder_stop(ad_sbus_responder_t *responder)
{
    delete responder;
}

extern "C" const char *ad_sbus_last_error(void)
{
    std::lock_guard<std::mutex> guard(g_last_error_lock);
    return g_last_error.c_str();
}
