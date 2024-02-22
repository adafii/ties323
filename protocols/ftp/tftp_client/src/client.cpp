#include "client.h"
#include "asio.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "utils.h"
#include <array>
#include <chrono>

using std::literals::string_view_literals::operator""sv;
using std::literals::chrono_literals::operator""ms;
using namespace asio::experimental::awaitable_operators;

namespace {

constexpr auto receive_timeout = 500ms;
constexpr auto buffer_size = std::size_t{1024};

asio::awaitable<asio::error_code> send_packet(asio::ip::udp::socket& socket,
                                              const asio::ip::udp::endpoint& receiver,
                                              const std::vector<std::uint8_t>& send_buffer) {
    auto [send_error, bytes_send] =
        co_await socket.async_send_to(asio::buffer(send_buffer), receiver, asio::as_tuple(asio::use_awaitable));

    if (send_error) {
        co_return send_error;
    }

    tftp::debug_packet("C: "sv, send_buffer, send_buffer.size());

    co_return asio::error_code{};
}

asio::awaitable<std::tuple<asio::error_code, asio::ip::udp::endpoint, std::size_t>> receive_packet(
    asio::ip::udp::socket& socket,
    std::array<std::uint8_t, buffer_size>& response,
    std::chrono::milliseconds wait_delay) {
    auto executor = co_await asio::this_coro::executor;
    auto sender_endpoint = asio::ip::udp::endpoint{};
    auto receive_error = asio::error_code{};

    auto read_status = co_await (socket.async_receive_from(asio::buffer(response), sender_endpoint,
                                                           asio::redirect_error(asio::use_awaitable, receive_error)) ||
                                 asio::steady_timer{executor, wait_delay}.async_wait(asio::use_awaitable));

    auto bytes_read = std::get_if<std::size_t>(&read_status);

    if (receive_error || bytes_read == nullptr) {
        co_return std::tuple{asio::error::timed_out, sender_endpoint, 0};
    }

    tftp::debug_packet("S: "sv, response, *bytes_read);

    co_return std::tuple{asio::error_code{}, sender_endpoint, *bytes_read};
}

}  // namespace

asio::awaitable<asio::error_code> tftp::write_client(asio::ip::udp::socket&& socket,
                                                     std::filesystem::path&& source,
                                                     asio::ip::udp::endpoint server) {
    auto executor = co_await asio::this_coro::executor;
    auto flags = asio::stream_file::read_only;
    auto file = asio::stream_file{executor, source.c_str(), flags};

    const auto request = to_buffer(tftp::packet::wrq{.filename{source.filename()}});
    const auto send_error = co_await send_packet(socket, server, request);

    if (send_error) {
        co_return send_error;
    }

    auto response = std::array<std::uint8_t, buffer_size>{};
    const auto [receive_error, sender_endpoint, bytes_read] =
        co_await receive_packet(socket, response, receive_timeout);

    if (receive_error) {
        debug("No response from server");
        co_return receive_error;
    }

    auto op = get_opcode(response);

    if (op != packet::opcode::ack) {
        debug("Unexpected response from server");
        co_return asio::error::connection_aborted;
    }

    server.port(sender_endpoint.port());

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
