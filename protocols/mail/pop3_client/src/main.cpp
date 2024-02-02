#include <asio.hpp>
#include <cstdio>

void client(asio::ip::tcp::socket socket) {}

int main(int argc, char* argv[]) {
    try {
        if (argc < 2 || argc > 3) {
            printf("Usage: pop3c <host> [ <port> ]\n");
            return 1;
        }

        std::string_view server = argv[1];
        std::string port = "110";

        if (argc == 3) {
            port = argv[2];
        }

        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        asio::ip::tcp::socket socket(io_context);
        asio::ip::tcp::resolver resolver(io_context);
        asio::connect(socket, resolver.resolve(server, port));

        client(std::move(socket));
    } catch (std::exception& e) {
        std::printf("Exception: %s\n", e.what());
    }
}