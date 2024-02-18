#include "asio.hpp"
#include "client.h"
#include "utils.h"
#include <chrono>
#include <iostream>

asio::awaitable<asio::error_code> init_client(std::string_view user,
                                              std::string_view password,
                                              const auto& host,
                                              const auto& port) {
    auto [error, socket] = co_await ftpc::connect_with_timeout(host, port, std::chrono::milliseconds{500});

    if (error) {
        co_return error;
    }

    co_return co_await ftpc::client(std::move(socket), user, password);
}

int main(int argc, char* argv[]) {
    auto args = std::vector<std::string>{argv + 1, argv + argc};

    if (args.size() != 4) {
        std::cerr << "Usage: " << argv[0] << " <user> <password> <host> <port>\n";
        return EXIT_FAILURE;
    }

    std::string_view user = args[0];
    std::string_view password = args[1];
    std::string_view host = args[2];
    std::string_view port = args[3];

    try {
        auto io_context = asio::io_context{10};

        auto signals = asio::signal_set{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        asio::co_spawn(io_context, init_client(user, password, host, port),
                       [&io_context](auto exception, const auto& error) {
                           if (exception) {
                               std::rethrow_exception(exception);
                           }

                           if (error) {
                               std::cerr << error.message() << '\n';
                           }

                           io_context.stop();
                       });

        io_context.run();

        std::cerr << "Disconnected\n";
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
    }

    return EXIT_SUCCESS;
}
