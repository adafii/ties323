#include "client.h"
#include "asio.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "utils.h"
#include <chrono>
#include <use_awaitable.hpp>

using std::literals::string_view_literals::operator""sv;
using std::literals::chrono_literals::operator""ms;
using namespace asio::experimental::awaitable_operators;

namespace {

constexpr auto receive_timeout = 500ms;
constexpr auto client_timeout = 4000ms;
constexpr auto buffer_size = std::size_t{1024};
constexpr auto data_size = std::size_t{512};

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

asio::awaitable<std::tuple<asio::error_code, asio::ip::udp::endpoint, std::size_t>>
receive_packet(asio::ip::udp::socket& socket, std::vector<std::uint8_t>& response, std::chrono::milliseconds timeout) {
    auto executor = co_await asio::this_coro::executor;
    auto sender_endpoint = asio::ip::udp::endpoint{};
    auto receive_error = asio::error_code{};

    response.resize(buffer_size);

    auto read_status =
        co_await (socket.async_receive_from(asio::buffer(response.data(), response.size()), sender_endpoint,
                                            asio::redirect_error(asio::use_awaitable, receive_error)) ||
                  asio::steady_timer{executor, timeout}.async_wait(asio::use_awaitable));

    auto bytes_read = std::get_if<std::size_t>(&read_status);

    if (receive_error || bytes_read == nullptr) {
        co_return std::tuple{asio::error::timed_out, sender_endpoint, 0};
    }

    response.resize(*bytes_read);

    tftp::debug_packet("S: "sv, response, *bytes_read);

    co_return std::tuple{asio::error_code{}, sender_endpoint, *bytes_read};
}

asio::awaitable<asio::error_code> send_loop(asio::ip::udp::socket& socket,
                                            const asio::ip::udp::endpoint& server,
                                            asio::stream_file& file,
                                            tftp::watchdog& send_loop_timer) {
    auto data_packet = tftp::packet::data{.data = std::vector<uint8_t>(data_size)};
    auto file_error = asio::error_code{};
    const auto file_reader = [&file, &file_error, &data_packet]() {
        return file.async_read_some(asio::mutable_buffer(data_packet.data.data(), data_size),
                                    asio::redirect_error(asio::use_awaitable, file_error));
    };

    auto response = std::vector<std::uint8_t>{};
    auto file_bytes_read = co_await file_reader();

    send_loop_timer.start(client_timeout);
    while (!file_error && file_bytes_read > 0) {
        data_packet.data.resize(file_bytes_read);
        const auto data = tftp::to_buffer(data_packet);
        const auto data_error = co_await send_packet(socket, server, data);

        if (data_error) {
            co_return data_error;
        }

        const auto [ack_receive_error, ack_sender_endpoint, ack_bytes_read] =
            co_await receive_packet(socket, response, receive_timeout);

        if (ack_receive_error) {
            co_return ack_receive_error;
        }

        const auto [opcode, block] = tftp::get_op_and_block(response);

        if (opcode != tftp::packet::opcode::ack || block != data_packet.block) {
            continue;
        }

        send_loop_timer.reset();

        ++data_packet.block;
        file_bytes_read = co_await file_reader();
    }

    co_return asio::error_code{};
}

}  // namespace

asio::awaitable<asio::error_code> tftp::write_client(asio::ip::udp::socket&& socket,
                                                     std::filesystem::path&& source,
                                                     asio::ip::udp::endpoint server) {
    auto executor = co_await asio::this_coro::executor;
    auto flags = asio::stream_file::read_only;
    auto file = asio::stream_file{executor, source.c_str(), flags};

    const auto write_request = to_buffer(tftp::packet::wrq{.filename{source.filename()}});
    const auto write_error = co_await send_packet(socket, server, write_request);

    if (write_error) {
        co_return write_error;
    }

    auto response = std::vector<std::uint8_t>{};
    const auto [receive_error, sender_endpoint, bytes_read] =
        co_await receive_packet(socket, response, receive_timeout);

    if (receive_error) {
        debug("No response from server, closing connection..."sv);
        co_return receive_error;
    }

    const auto [opcode, block] = get_op_and_block(response);

    if (opcode == packet::opcode::error) {
        debug("Server send error {}, closing connection...", block);
        co_return asio::error::connection_refused;
    }

    if (opcode != packet::opcode::ack || block != 0) {
        debug("Unexpected response from server"sv);
        co_return asio::error::connection_aborted;
    }

    server.port(sender_endpoint.port());

    auto send_loop_timer = watchdog{executor};
    auto send_loop_error =
        co_await asio::co_spawn(executor, send_loop(socket, server, file, send_loop_timer),
                                asio::bind_cancellation_slot(send_loop_timer.get_signal().slot(), asio::use_awaitable));

    if (send_loop_error) {
        co_return send_loop_error;
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
