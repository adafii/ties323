#pragma once
#include "asio/experimental/awaitable_operators.hpp"
#include "config.h"
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace ftpc {

using std::literals::string_view_literals::operator""sv;

constexpr auto client_debug = "C: {}"sv;
constexpr auto server_debug = "S: {}"sv;

template <typename... Args>
inline void debug(const std::format_string<Args...>& fmt, Args&&... args) {
    if constexpr (verbose) {
        std::cerr << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
    }
}

inline asio::awaitable<std::pair<asio::error_code, asio::ip::tcp::socket>>
connect_with_timeout(const auto& host, const auto& port, std::chrono::milliseconds timeout) {
    using namespace asio::experimental::awaitable_operators;

    const auto executor = co_await asio::this_coro::executor;
    auto socket = asio::ip::tcp::socket{executor};
    auto resolver = asio::ip::tcp::resolver{executor};

    auto resolve_error = asio::error_code{};
    const auto endpoint =
        co_await resolver.async_resolve(host, port, asio::redirect_error(asio::use_awaitable, resolve_error));

    if (resolve_error) {
        socket.close();
        co_return std::pair{resolve_error, std::move(socket)};
    }

    auto connect_error = asio::error_code{};
    co_await (asio::async_connect(socket, endpoint, asio::redirect_error(asio::use_awaitable, connect_error)) ||
              asio::steady_timer{executor, timeout}.async_wait(asio::as_tuple(asio::use_awaitable)));

    if (connect_error) {
        socket.close();
        if (connect_error == asio::error::operation_aborted) {
            connect_error = asio::error::timed_out;
        }
        co_return std::pair{connect_error, std::move(socket)};
    }

    co_return std::pair{asio::error_code{}, std::move(socket)};
}

}  // namespace ftpc