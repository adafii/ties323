#include "asio.hpp"
#include "asio/ssl.hpp"
#include "client.h"
#include "tls_socket.h"
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    try {
        if (argc != 5) {
            std::cerr << "Usage: " << argv[0] << " <user> <pass> <host> <port>\n";
            return EXIT_FAILURE;
        }

        std::string_view user = argv[1];
        std::string_view pass = argv[2];
        std::string_view host = argv[3];
        std::string_view port = argv[4];

        asio::io_context io_context(0);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        asio::ssl::context tls_context(asio::ssl::context::tlsv13);
        tls_context.set_default_verify_paths();

        tls_socket socket(io_context, tls_context);
        asio::ip::tcp::resolver resolver(io_context);

        asio::error_code connection_error{};
        asio::connect(socket.lowest_layer(), resolver.resolve(host, port), connection_error);

        if (connection_error) {
            std::cerr << "Connection error: " << connection_error.message() << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Connected to " << host << ":" << port << '\n';

        socket.set_verify_mode(asio::ssl::verify_peer);
        socket.set_verify_callback(asio::ssl::host_name_verification(std::string(host)));

        asio::error_code handshake_error{};
        socket.handshake(tls_socket::client, handshake_error);

        if (handshake_error) {
            std::cerr << "Handshake error: " << handshake_error.message() << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "TLS handshake OK\n";

        client(socket, user, pass);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
    }

    return EXIT_SUCCESS;
}