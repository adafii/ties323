#include "asio.hpp"
#include "server.h"
#include "utils.h"
#include <cstring>
#include <iostream>
#include <string_view>

using std::literals::string_view_literals::operator""sv;

constexpr auto RECEIVE_BUFFER_INITIAL_SIZE = std::size_t{128};

void connection_completion_handler(std::exception_ptr exception_ptr,
                                   std::pair<asio::ip::udp::endpoint, asio::error_code>&& return_value) {

    if (exception_ptr) {
        try {
            std::rethrow_exception(exception_ptr);
        } catch (std::exception& exception) {
            tftp::debug("Connection ended with exception: {}", exception.what());
        }
    }

    const auto [client, error] = return_value;

    if (error) {
        tftp::debug("Connection from {}:{} returned error: {}", client.address().to_string(), client.port(),
                    error.message());
        return;
    }

    tftp::debug("Closing connection to client {}:{}", client.address().to_string(), client.port());
}

asio::awaitable<asio::error_code> spawn_connections(asio::ip::udp::socket&& server_socket) {
    auto executor = server_socket.get_executor();

    for (;;) {
        auto request_buffer = std::make_unique<std::vector<std::uint8_t>>(RECEIVE_BUFFER_INITIAL_SIZE);

        auto wait_error = asio::error_code{};
        co_await server_socket.async_wait(asio::ip::udp::socket::wait_read,
                                          asio::redirect_error(asio::use_awaitable, wait_error));

        if (wait_error) {
            co_return wait_error;
        }

        if (const auto bytes_available = server_socket.available(); bytes_available > RECEIVE_BUFFER_INITIAL_SIZE) {
            request_buffer->resize(bytes_available);
        }

        auto client_endpoint = asio::ip::udp::endpoint{};
        auto receive_error = asio::error_code{};
        auto bytes_read = co_await server_socket.async_receive_from(
            asio::buffer(*request_buffer, request_buffer->size()), client_endpoint,
            asio::redirect_error(asio::use_awaitable, receive_error));

        request_buffer->resize(bytes_read);

        if (receive_error) {
            co_return receive_error;
        }

        auto opcode = tftp::get_opcode(*request_buffer);

        switch (opcode) {
            case tftp::packet::opcode::rrq: {
                tftp::debug("Received read request from {}:{}", client_endpoint.address().to_string(),
                            client_endpoint.port());
                tftp::debug_packet("C: "sv, *request_buffer);

                asio::co_spawn(executor, tftp::read_connection(client_endpoint, std::move(request_buffer)),
                               connection_completion_handler);
                break;
            }
            case tftp::packet::opcode::wrq: {
                tftp::debug("Received write request from {}:{}", client_endpoint.address().to_string(),
                            client_endpoint.port());
                tftp::debug_packet("C: "sv, *request_buffer);

                asio::co_spawn(executor, tftp::write_connection(client_endpoint, std::move(request_buffer)),
                               connection_completion_handler);
                break;
            }
            default: {
                tftp::debug("Received invalid request from {}:{}\n", client_endpoint.address().to_string(),
                            client_endpoint.port());
                break;
            }
        }
    }
}

int main() {
    auto server_port = "6900"sv;

    try {
        auto io_context = asio::io_context{};

        auto signals = asio::signal_set{io_context, SIGINT, SIGHUP, SIGTERM};
        signals.async_wait([&io_context]([[maybe_unused]] auto error, auto signal_number) {
            std::cerr << std::format("\n{}, quitting...\n", strsignal(signal_number));
            io_context.stop();
        });

        auto resolve_error = asio::error_code{};
        auto ports = asio::ip::udp::resolver{io_context}.resolve("", server_port, resolve_error);

        if (resolve_error) {
            std::cerr << std::format("Could not resolve port {}", server_port);
            return EXIT_FAILURE;
        }

        auto port = (*ports.begin()).endpoint().port();
        auto endpoint = asio::ip::udp::endpoint{asio::ip::udp::v4(), port};
        auto server_socket = asio::ip::udp::socket{io_context, endpoint};
        server_socket.set_option(asio::socket_base::receive_buffer_size{8192});

        asio::co_spawn(io_context, spawn_connections(std::move(server_socket)),
                       [&io_context](auto exception_ptr, const auto error) {
                           if (exception_ptr) {
                               std::rethrow_exception(exception_ptr);
                           }

                           if (error) {
                               std::cerr << std::format("Server returned error: {}\n", error.message());
                           }

                           io_context.stop();
                       });

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << std::format("Exception: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
