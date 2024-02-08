#include "asio.hpp"
#include "asio/ssl.hpp"
#include "client.h"
#include "tls_socket.h"
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    auto args = std::vector<std::string>{argv + 1, argv + argc};

    if (args.size() < 4 || args.size() > 5) {
        std::cerr << "Usage: " << argv[0] << " [ -tls ] <user> <pass> <host> <port>\n";
        return EXIT_FAILURE;
    }

    auto it = args.begin();

    auto tls = false;
    if (args.size() == 5 && *it == "-tls") {
        tls = true;
        ++it;
    }

    std::string_view user = *std::next(it, 0);
    std::string_view pass = *std::next(it, 1);
    std::string_view host = *std::next(it, 2);
    std::string_view port = *std::next(it, 3);

    try {
        auto io_context = asio::io_context{1};

        auto signals = asio::signal_set(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        if (tls) {
            auto tls_context = asio::ssl::context{asio::ssl::context::tlsv13};
            tls_context.set_default_verify_paths();

            auto tls_socket = tls_socket_t{io_context, tls_context};
            auto resolver = asio::ip::tcp::resolver(io_context);

            auto connection_error = asio::error_code{};
            asio::connect(tls_socket.lowest_layer(), resolver.resolve(host, port), connection_error);

            if (connection_error) {
                std::cerr << "Connection error: " << connection_error.message() << '\n';
                return EXIT_FAILURE;
            }

            std::cout << "Connected to " << host << ":" << port << '\n';

            tls_socket.set_verify_mode(asio::ssl::verify_peer);
            tls_socket.set_verify_callback(asio::ssl::host_name_verification(std::string(host)));

            auto handshake_error = asio::error_code{};
            tls_socket.handshake(tls_socket_t::client, handshake_error);

            if (handshake_error) {
                std::cerr << "Handshake error: " << handshake_error.message() << '\n';
                return EXIT_FAILURE;
            }

            std::cout << "TLS handshake OK\n";
            client(tls_socket, user, pass);
        } else {
            auto socket = asio::ip::tcp::socket{io_context};
            auto resolver = asio::ip::tcp::resolver{io_context};

            auto connection_error = asio::error_code{};
            asio::connect(socket, resolver.resolve(host, port), connection_error);

            if (connection_error) {
                std::cerr << "Connection error: " << connection_error.message() << '\n';
                return EXIT_FAILURE;
            }

            std::cout << "Connected to " << host << ":" << port << '\n';
            client(socket, user, pass);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
    }

    return EXIT_SUCCESS;
}