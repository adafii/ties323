#include "client.h"
#include "asio.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "utils.h"
#include <chrono>

using std::literals::string_view_literals::operator""sv;
using std::literals::chrono_literals::operator""ms;
using namespace asio::experimental::awaitable_operators;

namespace {

constexpr auto receive_timeout = 1000ms;
constexpr auto client_watchdog_timeout = 5000ms;
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

asio::awaitable<std::tuple<asio::error_code, asio::ip::udp::endpoint>> receive_packet(
    asio::ip::udp::socket& socket,
    std::vector<std::uint8_t>& response_buffer,
    std::chrono::milliseconds timeout) {
    auto executor = co_await asio::this_coro::executor;
    auto sender_endpoint = asio::ip::udp::endpoint{};
    auto receive_error = asio::error_code{};

    response_buffer.resize(buffer_size);

    auto read_status = co_await (socket.async_receive_from(asio::buffer(response_buffer.data(), response_buffer.size()),
                                                           sender_endpoint,
                                                           asio::redirect_error(asio::use_awaitable, receive_error)) ||
                                 asio::steady_timer{executor, timeout}.async_wait(asio::use_awaitable));

    auto bytes_read = std::get_if<std::size_t>(&read_status);

    if (receive_error || bytes_read == nullptr) {
        co_return std::tuple{asio::error::timed_out, sender_endpoint};
    }

    response_buffer.resize(*bytes_read);

    tftp::debug_packet("S: "sv, response_buffer, *bytes_read);

    co_return std::tuple{asio::error_code{}, sender_endpoint};
}

asio::awaitable<std::pair<asio::error_code, asio::ip::udp::endpoint>> handle_server_response(
    asio::ip::udp::socket& socket,
    const asio::ip::udp::endpoint& server,
    std::vector<uint8_t>& response_buffer) {
    const auto [receive_error, sender_endpoint] = co_await receive_packet(socket, response_buffer, receive_timeout);

    if (receive_error == asio::error::timed_out) {
        tftp::debug("No response from server, closing connection..."sv);
        co_return std::pair{receive_error, sender_endpoint};
    }

    if (receive_error) {
        co_return std::pair{receive_error, sender_endpoint};
    }

    const auto [opcode, error_code, error_message] = tftp::get_error(response_buffer);

    if (opcode == tftp::packet::opcode::error) {
        tftp::debug("Server sent error {}: {}"sv, error_code, error_message);
        co_return std::pair{asio::error::connection_refused, sender_endpoint};
    }

    co_return std::pair{asio::error_code{}, sender_endpoint};
}

asio::awaitable<asio::error_code> send_loop(asio::ip::udp::socket& socket,
                                            const asio::ip::udp::endpoint& server,
                                            asio::stream_file& file,
                                            tftp::watchdog& send_loop_watchdog) {
    auto response_buffer = std::vector<std::uint8_t>{};
    auto data_packet = tftp::packet::data{.data = std::vector<uint8_t>(data_size)};
    auto file_error = asio::error_code{};

    auto file_bytes_read = co_await file.async_read_some(asio::mutable_buffer(data_packet.data.data(), data_size),
                                                         asio::redirect_error(asio::use_awaitable, file_error));

    send_loop_watchdog.start(client_watchdog_timeout);

    while (!file_error && file_bytes_read > 0) {
        data_packet.data.resize(file_bytes_read);
        const auto data = tftp::to_buffer(data_packet);
        const auto send_data_error = co_await send_packet(socket, server, data);

        if (send_data_error) {
            co_return send_data_error;
        }

        const auto [ack_receive_error, ack_sender_endpoint] =
            co_await receive_packet(socket, response_buffer, receive_timeout);

        // No ack to data -> resend data
        if (ack_receive_error == asio::error::timed_out) {
            continue;
        }

        if (ack_receive_error) {
            co_return ack_receive_error;
        }

        if (ack_sender_endpoint.port() != server.port()) {
            co_await send_packet(socket, server, tftp::to_buffer(tftp::packet::error{.error_code = 5, .error_msg{"Unknown transfer ID."sv}}));
            continue;
        }

        const auto [opcode, block] = tftp::get_op_and_block(response_buffer);

        // Wrong opcode or block number -> resend data
        if (opcode != tftp::packet::opcode::ack || block != data_packet.block) {
            continue;
        }

        send_loop_watchdog.reset();

        ++data_packet.block;
        file_bytes_read = co_await file.async_read_some(asio::mutable_buffer(data_packet.data.data(), data_size),
                                                        asio::redirect_error(asio::use_awaitable, file_error));
    }

    co_return asio::error_code{};
}

asio::awaitable<asio::error_code> receive_loop(asio::ip::udp::socket& socket,
                                               const asio::ip::udp::endpoint& server,
                                               asio::stream_file& file,
                                               tftp::watchdog& receive_loop_watchdog,
                                               std::vector<std::uint8_t>& receive_buffer) {
    auto file_error = asio::error_code{};
    auto current_block = tftp::packet::block_t{1};

    auto data_from_current_response = tftp::get_data_from_packet(receive_buffer);
    co_await file.async_write_some(asio::buffer(data_from_current_response.data(), data_from_current_response.size()),
                                   asio::redirect_error(asio::use_awaitable, file_error));

    receive_loop_watchdog.start(client_watchdog_timeout);

    while (!file_error) {
        const auto ack = tftp::to_buffer(tftp::packet::ack{.block = current_block});
        const auto send_ack_error = co_await send_packet(socket, server, ack);

        if (send_ack_error) {
            co_return send_ack_error;
        }

        const auto [data_receive_error, data_sender_endpoint] =
            co_await receive_packet(socket, receive_buffer, receive_timeout);

        // The last data packet was received and server didn't try to resend it after ack -> finished
        if (data_receive_error == asio::error::timed_out && data_from_current_response.size() < data_size) {
            break;
        }

        // No response to ack -> resend ack
        if (data_receive_error == asio::error::timed_out) {
            continue;
        }

        if (data_receive_error) {
            co_return data_receive_error;
        }

        if (data_sender_endpoint.port() != server.port()) {
            co_await send_packet(socket, server, tftp::to_buffer(tftp::packet::error{.error_code = 5, .error_msg{"Unknown transfer ID."sv}}));
            continue;
        }

        const auto [opcode, block] = tftp::get_op_and_block(receive_buffer);

        // Wrong opcode or block number -> resend ack
        if (opcode != tftp::packet::opcode::data || block != current_block + 1) {
            continue;
        }

        receive_loop_watchdog.reset();

        data_from_current_response = tftp::get_data_from_packet(receive_buffer);
        co_await file.async_write_some(
            asio::buffer(data_from_current_response.data(), data_from_current_response.size()),
            asio::redirect_error(asio::use_awaitable, file_error));

        ++current_block;
    }

    co_return asio::error_code{};
}

}  // namespace

asio::awaitable<asio::error_code> tftp::write_client(asio::ip::udp::socket&& socket,
                                                     std::filesystem::path&& source,
                                                     asio::ip::udp::endpoint server) {
    debug("Writing {} to {}:{}"sv, source.string(), server.address().to_string(), server.port());

    const auto write_request = to_buffer(packet::wrq{.filename{source.filename()}});
    const auto write_request_error = co_await send_packet(socket, server, write_request);

    if (write_request_error) {
        debug("Write request failed"sv);
        co_return write_request_error;
    }

    auto write_request_response_buffer = std::vector<uint8_t>{};
    const auto [server_response_error, sender_endpoint] =
        co_await handle_server_response(socket, server, write_request_response_buffer);

    if (server_response_error) {
        co_return server_response_error;
    }

    const auto [opcode, block] = get_op_and_block(write_request_response_buffer);

    if (opcode != packet::opcode::ack || block != 0) {
        tftp::debug("Unexpected response from server"sv);
        co_return asio::error::connection_aborted;
    }

    server.port(sender_endpoint.port());

    auto executor = co_await asio::this_coro::executor;
    auto flags = asio::stream_file::read_only;
    auto file = asio::stream_file{executor, source.c_str(), flags};

    auto send_loop_watchdog = watchdog{executor};
    auto send_loop_error = co_await asio::co_spawn(
        executor, send_loop(socket, server, file, send_loop_watchdog),
        asio::bind_cancellation_slot(send_loop_watchdog.get_signal_slot(), asio::use_awaitable));

    if (send_loop_error) {
        co_return send_loop_error;
    }

    debug("Writing succeeded, disconnecting...");

    co_return asio::error_code{};
}

asio::awaitable<asio::error_code> tftp::read_client(asio::ip::udp::socket&& socket,
                                                    std::filesystem::path&& target,
                                                    asio::ip::udp::endpoint server) {
    debug("Reading {} from {}:{}"sv, target.string(), server.address().to_string(), server.port());

    const auto read_request = to_buffer(packet::rrq{.filename{target.filename()}});
    const auto read_request_error = co_await send_packet(socket, server, read_request);

    if (read_request_error) {
        debug("Read request failed"sv);
        co_return read_request_error;
    }

    auto read_request_response_buffer = std::vector<uint8_t>{};
    const auto [server_response_error, sender_endpoint] =
        co_await handle_server_response(socket, server, read_request_response_buffer);

    if (server_response_error) {
        co_return server_response_error;
    }

    const auto [opcode, block] = tftp::get_op_and_block(read_request_response_buffer);

    if (opcode != tftp::packet::opcode::data || block != 1) {
        tftp::debug("Unexpected response from server"sv);
        co_return asio::error::connection_aborted;
    }

    server.port(sender_endpoint.port());

    auto executor = co_await asio::this_coro::executor;
    auto flags = asio::stream_file::write_only | asio::stream_file::create | asio::stream_file::exclusive;
    auto file = asio::stream_file{executor, target, flags};
    std::filesystem::permissions(target, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    auto read_loop_watchdog = watchdog{executor};
    auto read_loop_error = co_await asio::co_spawn(
        executor, receive_loop(socket, server, file, read_loop_watchdog, read_request_response_buffer),
        asio::bind_cancellation_slot(read_loop_watchdog.get_signal_slot(), asio::use_awaitable));

    if (read_loop_error) {
        std::filesystem::remove(target);
        co_return read_loop_error;
    }

    debug("Reading succeeded, disconnecting...");

    co_return asio::error_code{};
}
