#include "asio.hpp"
#include "server.h"
#include <iostream>
#include <string_view>
#include <cstring>

using std::literals::string_view_literals::operator""sv;

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

        asio::co_spawn(io_context, tftp::spawn_connections(std::move(server_socket)),
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
