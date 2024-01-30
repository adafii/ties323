#include "smtp.h"
#include <asio.hpp>

asio::awaitable<void> listener(asio::ip::tcp::acceptor acceptor) {
    for (;;) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        co_spawn(socket.get_executor(), smtp_session(std::move(socket)), asio::detached);
    }
}

int main() {
    try {
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        co_spawn(io_context, listener(asio::ip::tcp::acceptor{io_context, {asio::ip::tcp::v4(), 2525}}),
                 asio::detached);

        io_context.run();
    } catch (std::exception& e) {
        std::printf("Exception: %s\n", e.what());
    }
}