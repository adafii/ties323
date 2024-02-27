#include "server.h"
#include "utils.h"

using std::literals::string_view_literals::operator""sv;

namespace {

constexpr auto RECEIVE_BUFFER_INITIAL_SIZE = std::size_t{128};

void connection_completion_handler(std::exception_ptr exception_ptr,
                                   std::pair<asio::ip::udp::endpoint, asio::error_code>&& return_value) {
    if (exception_ptr) {
        std::rethrow_exception(exception_ptr);
    }

    const auto [client, error] = return_value;

    if (error) {
        tftp::debug("Connection from {}:{} returned error: {}", client.address().to_string(), client.port(),
                    error.message());
    }
};

}  // namespace

namespace tftp {

asio::awaitable<asio::error_code> spawn_connections(asio::ip::udp::socket&& server_socket) {
    auto executor = co_await asio::this_coro::executor;

    for (;;) {
        auto receive_buffer = std::make_unique<std::vector<std::uint8_t>>(RECEIVE_BUFFER_INITIAL_SIZE);

        auto wait_error = asio::error_code{};
        co_await server_socket.async_wait(asio::ip::udp::socket::wait_read,
                                          asio::redirect_error(asio::use_awaitable, wait_error));

        if (wait_error) {
            co_return wait_error;
        }

        if (const auto bytes_available = server_socket.available(); bytes_available > RECEIVE_BUFFER_INITIAL_SIZE) {
            receive_buffer->resize(bytes_available);
        }

        auto client_endpoint = asio::ip::udp::endpoint{};
        auto receive_error = asio::error_code{};
        auto bytes_read = co_await server_socket.async_receive_from(
            asio::buffer(*receive_buffer, receive_buffer->size()), client_endpoint,
            asio::redirect_error(asio::use_awaitable, receive_error));

        receive_buffer->resize(bytes_read);

        if (receive_error) {
            co_return receive_error;
        }

        auto opcode = tftp::get_opcode(*receive_buffer);

        switch (opcode) {
            case tftp::packet::opcode::rrq: {
                debug("Received read request from {}:{}", client_endpoint.address().to_string(),
                      client_endpoint.port());
                debug_packet("C: "sv, *receive_buffer);

                asio::co_spawn(executor,
                               tftp::read_connection(asio::ip::udp::socket{executor, asio::ip::udp::v4()},
                                                     std::move(client_endpoint), std::move(receive_buffer)),
                               connection_completion_handler);
                break;
            }
            case tftp::packet::opcode::wrq: {
                debug("Received write request from {}:{}", client_endpoint.address().to_string(),
                      client_endpoint.port());
                debug_packet("C: "sv, *receive_buffer);

                asio::co_spawn(executor,
                               tftp::write_connection(asio::ip::udp::socket{executor, asio::ip::udp::v4()},
                                                     std::move(client_endpoint), std::move(receive_buffer)),
                               connection_completion_handler);
                break;
            }
            default: {
                debug("Received invalid request from {}:{}\n", client_endpoint.address().to_string(),
                      client_endpoint.port());
                break;
            }
        }
    }
}

asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> read_connection(
    asio::ip::udp::socket&& connection_socket,
    asio::ip::udp::endpoint&& client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer) {
    co_return std::pair{client_endpoint, asio::error_code{}};
}

asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> write_connection(
    asio::ip::udp::socket&& connection_socket,
    asio::ip::udp::endpoint&& client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer) {
    co_return std::pair{client_endpoint, asio::error_code{}};
}

}  // namespace tftp