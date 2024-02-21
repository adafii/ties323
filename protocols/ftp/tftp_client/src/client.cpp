#include "client.h"
#include "asio.hpp"
#include "utils.h"
#include <iostream>

namespace {}  // namespace

asio::awaitable<asio::error_code> tftp::write_client(asio::ip::udp::socket&& socket,
                                                     std::filesystem::path&& source,
                                                     asio::ip::udp::endpoint server) {
    auto flags = asio::stream_file::read_only;
    auto file = asio::stream_file{co_await asio::this_coro::executor, source.c_str(), flags};

    const auto request = to_buffer(packet::wrq{.filename{source.filename()}});

    auto send_error = asio::error_code{};
    co_await socket.async_send_to(asio::buffer(request), server, asio::redirect_error(asio::use_awaitable, send_error));

    if (send_error) {
        co_return send_error;
    }

    co_return asio::error_code{};
}

asio::awaitable<asio::error_code> tftp::read_client(asio::ip::udp::socket&& socket,
                                                    std::filesystem::path&& target,
                                                    asio::ip::udp::endpoint server) {
    auto flags = asio::stream_file::write_only | asio::stream_file::create | asio::stream_file::exclusive;
    auto file = asio::stream_file{co_await asio::this_coro::executor, target, flags};
    std::filesystem::permissions(target, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    co_return asio::error_code{};
}
