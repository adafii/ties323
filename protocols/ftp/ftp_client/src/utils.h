#pragma once
#include <chrono>

namespace ftpc {

constexpr auto connect_wait = std::chrono::seconds{1};
constexpr auto read_wait = std::chrono::seconds{5};

inline asio::awaitable<std::pair<asio::error_code, asio::ip::tcp::socket>>
connect_with_timeout(const auto& host, const auto& port) {
    using namespace asio::experimental::awaitable_operators;

    auto executor = co_await asio::this_coro::executor;
    auto socket = asio::ip::tcp::socket{executor};
    auto resolver = asio::ip::tcp::resolver{executor};

    auto resolve_error = asio::error_code{};
    auto endpoint = co_await resolver.async_resolve(host, port, asio::redirect_error(asio::use_awaitable, resolve_error));

    if (resolve_error) {
        socket.close();
        co_return std::pair{resolve_error, std::move(socket)};
    }

    asio::error_code connect_error{};
    co_await (asio::async_connect(socket, endpoint, asio::redirect_error(asio::use_awaitable, connect_error)) ||
              asio::steady_timer{executor, connect_wait}.async_wait(asio::as_tuple(asio::use_awaitable)));

    if (connect_error) {
        socket.close();
        if (connect_error == asio::error::operation_aborted) {
            connect_error = asio::error::timed_out;
        }
        co_return std::pair{connect_error, std::move(socket)};
    }

    co_return std::pair{asio::error_code{}, std::move(socket)};
};

}  // namespace ftpc