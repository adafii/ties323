#include "client.h"
#include "asio.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "utils.h"
#include <chrono>
#include <fstream>

namespace {

using std::literals::string_view_literals::operator""sv;
using std::literals::chrono_literals::operator""ms;
using namespace asio::experimental::awaitable_operators;

constexpr auto MAX_RECEIVE_SIZE = std::size_t{1024};
constexpr auto DATA_CHUNK_SIZE = std::size_t{512};
constexpr auto MAX_RECEIVE_WAIT_TIME = 2000ms;
constexpr auto MAX_WAIT_TIME = 10000ms;

asio::awaitable<asio::error_code> send_packet(asio::ip::udp::socket& socket,
                                              const asio::ip::udp::endpoint& receiver,
                                              const std::vector<std::uint8_t>& send_buffer) {

    auto send_error = asio::error_code{};
    co_await socket.async_send_to(asio::buffer(send_buffer), receiver,
                                  asio::redirect_error(asio::use_awaitable, send_error));

    if (send_error) {
        co_return send_error;
    }

    tftp::debug_packet("C: "sv, send_buffer);

    co_return asio::error_code{};
}

asio::awaitable<std::tuple<asio::error_code, asio::ip::udp::endpoint>> receive_packet(
    asio::ip::udp::socket& socket,
    std::vector<std::uint8_t>& receive_buffer,
    std::chrono::milliseconds timeout) {

    auto executor = co_await asio::this_coro::executor;
    auto sender_endpoint = asio::ip::udp::endpoint{};
    auto receive_error = asio::error_code{};

    receive_buffer.resize(MAX_RECEIVE_SIZE);

    auto read_status =
        co_await (socket.async_receive_from(asio::buffer(receive_buffer.data(), receive_buffer.size()), sender_endpoint,
                                            asio::redirect_error(asio::use_awaitable, receive_error)) ||
                  asio::steady_timer{executor, timeout}.async_wait(asio::use_awaitable));

    auto bytes_read = std::get_if<std::size_t>(&read_status);

    if (receive_error || bytes_read == nullptr) {
        co_return std::tuple{asio::error::timed_out, sender_endpoint};
    }

    receive_buffer.resize(*bytes_read);

    tftp::debug_packet("S: "sv, receive_buffer);

    co_return std::tuple{asio::error_code{}, sender_endpoint};
}

asio::awaitable<std::pair<asio::error_code, asio::ip::udp::endpoint>> handle_server_response(
    asio::ip::udp::socket& socket,
    std::vector<uint8_t>& response_buffer) {

    const auto [receive_error, sender_endpoint] =
        co_await receive_packet(socket, response_buffer, MAX_RECEIVE_WAIT_TIME);

    if (receive_error == asio::error::timed_out) {
        tftp::debug("No response from server, closing connection..."sv);
        co_return std::pair{receive_error, sender_endpoint};
    }

    if (receive_error) {
        co_return std::pair{receive_error, sender_endpoint};
    }

    const auto [opcode, error_code, error_message] = tftp::get_error(response_buffer);

    if (opcode == tftp::packet::opcode::error) {
        tftp::debug("Server sent error: {} - {}"sv, error_code, error_message);
        co_return std::pair{asio::error::connection_refused, sender_endpoint};
    }

    co_return std::pair{asio::error_code{}, sender_endpoint};
}

asio::awaitable<asio::error_code> send_loop(asio::ip::udp::socket& socket,
                                            const asio::ip::udp::endpoint& server,
                                            std::ifstream& file_stream) {

    auto file_reader = [&file_stream](std::vector<std::uint8_t>& data) {
        file_stream.read(std::bit_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return static_cast<std::size_t>(file_stream.gcount());
    };

    auto receive_buffer = std::vector<std::uint8_t>{};
    auto data_packet = tftp::packet::data{.data = std::vector<uint8_t>(DATA_CHUNK_SIZE)};
    auto file_bytes_read = file_reader(data_packet.data);

    auto send_loop_watchdog = tftp::watchdog{socket.get_executor(), MAX_WAIT_TIME};

    while (file_bytes_read > 0) {
        if (send_loop_watchdog.has_expired()) {
            co_return asio::error::timed_out;
        }

        data_packet.data.resize(file_bytes_read);
        const auto data = tftp::to_buffer(data_packet);
        const auto send_data_error = co_await send_packet(socket, server, data);

        if (send_data_error) {
            co_return send_data_error;
        }

        const auto [ack_receive_error, ack_sender_endpoint] =
            co_await receive_packet(socket, receive_buffer, MAX_RECEIVE_WAIT_TIME);

        // No ack to data -> resend data
        if (ack_receive_error == asio::error::timed_out) {
            continue;
        }

        if (ack_receive_error) {
            co_return ack_receive_error;
        }

        // Wrong packet origin -> send error to origin and ignore the packet
        if (ack_sender_endpoint != server) {
            co_await send_packet(
                socket, ack_sender_endpoint,
                tftp::to_buffer(tftp::packet::error{.error_code = 5, .error_msg{"Unknown transfer ID."sv}}));
            continue;
        }

        const auto opcode = tftp::get_opcode(receive_buffer);
        const auto block_number = tftp::get_block_number(receive_buffer);

        // Wrong opcode or block number -> resend data
        if (opcode != tftp::packet::opcode::ack || block_number != data_packet.block) {
            continue;
        }

        send_loop_watchdog.reset();

        file_bytes_read = file_reader(data_packet.data);
        ++data_packet.block;
    }

    if (!file_stream.eof()) {
        tftp::debug("EOF was not reached when reading file");
        co_return asio::error::fault;
    }

    co_return asio::error_code{};
}

asio::awaitable<asio::error_code> receive_loop(asio::ip::udp::socket& socket,
                                               const asio::ip::udp::endpoint& server,
                                               std::ofstream& file_stream,
                                               std::vector<std::uint8_t>& receive_buffer) {

    auto current_block = tftp::packet::block_t{1};
    auto data_from_current_response = tftp::get_data(receive_buffer);
    file_stream.write(std::bit_cast<char*>(data_from_current_response.data()),
                      static_cast<std::streamsize>(data_from_current_response.size()));

    auto receive_loop_watchdog = tftp::watchdog{socket.get_executor(), MAX_WAIT_TIME};

    while (file_stream.good()) {
        if (receive_loop_watchdog.has_expired()) {
            co_return asio::error::timed_out;
        }

        const auto ack = tftp::to_buffer(tftp::packet::ack{.block = current_block});
        const auto send_ack_error = co_await send_packet(socket, server, ack);

        if (send_ack_error) {
            co_return send_ack_error;
        }

        const auto [data_receive_error, data_sender_endpoint] =
            co_await receive_packet(socket, receive_buffer, MAX_RECEIVE_WAIT_TIME);

        // The last data packet was received and server didn't try to resend it after ack -> finished
        if (data_receive_error == asio::error::timed_out && data_from_current_response.size() < DATA_CHUNK_SIZE) {
            break;
        }

        // No response to ack -> resend ack
        if (data_receive_error == asio::error::timed_out) {
            continue;
        }

        if (data_receive_error) {
            co_return data_receive_error;
        }

        // Wrong packet origin -> send error to origin and ignore the packet
        if (data_sender_endpoint != server) {
            co_await send_packet(
                socket, data_sender_endpoint,
                tftp::to_buffer(tftp::packet::error{.error_code = 5, .error_msg{"Unknown transfer ID."sv}}));
            continue;
        }

        const auto opcode = tftp::get_opcode(receive_buffer);
        const auto block_number = tftp::get_block_number(receive_buffer);

        // Wrong opcode or block number -> resend ack
        if (opcode != tftp::packet::opcode::data || block_number != current_block + 1) {
            continue;
        }

        receive_loop_watchdog.reset();

        data_from_current_response = tftp::get_data(receive_buffer);
        file_stream.write(std::bit_cast<char*>(data_from_current_response.data()),
                          static_cast<std::streamsize>(data_from_current_response.size()));
        ++current_block;
    }

    if (!file_stream.good()) {
        tftp::debug("Error while writing file");
        co_return asio::error::fault;
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
        co_await handle_server_response(socket, write_request_response_buffer);

    if (server_response_error) {
        co_return server_response_error;
    }

    const auto opcode = tftp::get_opcode(write_request_response_buffer);
    const auto block_number = tftp::get_block_number(write_request_response_buffer);

    if (opcode != packet::opcode::ack || block_number != 0) {
        tftp::debug("Unexpected response from server"sv);
        co_return asio::error::connection_aborted;
    }

    server.port(sender_endpoint.port());

    auto executor = co_await asio::this_coro::executor;
    auto file_stream = std::ifstream{source, std::ios::in | std::ios::binary};

    auto send_loop_error = co_await asio::co_spawn(
        executor, send_loop(socket, server, file_stream), asio::use_awaitable);

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
        co_await handle_server_response(socket, read_request_response_buffer);

    if (server_response_error) {
        co_return server_response_error;
    }

    const auto opcode = tftp::get_opcode(read_request_response_buffer);
    const auto block_number = tftp::get_block_number(read_request_response_buffer);

    if (opcode != tftp::packet::opcode::data || block_number != 1) {
        tftp::debug("Unexpected response from server"sv);
        co_return asio::error::connection_aborted;
    }

    server.port(sender_endpoint.port());

    auto executor = co_await asio::this_coro::executor;
    auto file_stream = std::ofstream{target, std::ios::out | std::ios::binary};
    std::filesystem::permissions(target, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    auto receive_loop_error = co_await asio::co_spawn(
        executor, receive_loop(socket, server, file_stream, read_request_response_buffer), asio::use_awaitable);

    if (receive_loop_error) {
        std::filesystem::remove(target);
        co_return receive_loop_error;
    }

    debug("Reading succeeded, disconnecting...");

    co_return asio::error_code{};
}
