#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>
#include <cstdio>

asio::awaitable<void> echo(asio::ip::tcp::socket socket) {
    try {
        char data[128];
        for (;;) {
            std::size_t n = co_await socket.async_read_some(asio::buffer(data), asio::use_awaitable);
            co_await async_write(socket, asio::buffer(data, n), asio::use_awaitable);
        }
    } catch (std::exception& e) {
        std::printf("echo Exception: %s\n", e.what());
    }
}

asio::awaitable<void> listener() {
    auto executor = co_await asio::this_coro::executor;
    asio::ip::tcp::acceptor acceptor(executor, {asio::ip::tcp::v4(), 55555});
    for (;;) {
        asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
        co_spawn(executor, echo(std::move(socket)), asio::detached);
    }
}

int main() {
    try {
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        co_spawn(io_context, listener(), asio::detached);

        io_context.run();
    } catch (std::exception& e) {
        std::printf("Exception: %s\n", e.what());
    }
}