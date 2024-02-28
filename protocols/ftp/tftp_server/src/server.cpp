#include "server.h"
#include "asio/experimental/awaitable_operators.hpp"
#include "utils.h"
#include <filesystem>
#include <fstream>

namespace {

using std::literals::string_literals::operator""s;
using std::literals::string_view_literals::operator""sv;
using std::literals::chrono_literals::operator""ms;
using namespace asio::experimental::awaitable_operators;

constexpr auto DATA_CHUNK_SIZE = std::size_t{512};
constexpr auto MAX_RECEIVE_SIZE = std::size_t{1024};
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

    tftp::debug_packet("S: "sv, send_buffer);

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
        co_await (socket.async_receive_from(asio::buffer(receive_buffer, receive_buffer.size()), sender_endpoint,
                                            asio::redirect_error(asio::use_awaitable, receive_error)) ||
                  asio::steady_timer{executor, timeout}.async_wait(asio::use_awaitable));

    auto bytes_read = std::get_if<std::size_t>(&read_status);

    if (receive_error || bytes_read == nullptr) {
        co_return std::tuple{asio::error::timed_out, sender_endpoint};
    }

    receive_buffer.resize(*bytes_read);

    tftp::debug_packet("C: "sv, receive_buffer);

    co_return std::tuple{asio::error_code{}, sender_endpoint};
}

asio::awaitable<asio::error_code> send_loop(asio::ip::udp::socket& connection_socket,
                                            const asio::ip::udp::endpoint& client_endpoint,
                                            std::ifstream& file_stream) {

    auto receive_buffer = std::vector<std::uint8_t>{};
    auto data_packet = tftp::packet::data{.data = std::vector<uint8_t>(DATA_CHUNK_SIZE)};

    auto file_reader = [&file_stream](std::vector<std::uint8_t>& data) {
        file_stream.read(std::bit_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return static_cast<std::size_t>(file_stream.gcount());
    };

    auto file_bytes_read = file_reader(data_packet.data);
    auto send_loop_watchdog = tftp::watchdog{connection_socket.get_executor(), MAX_WAIT_TIME};

    while (file_bytes_read > 0) {
        if (send_loop_watchdog.has_expired()) {
            co_return asio::error::timed_out;
        }

        data_packet.data.resize(file_bytes_read);
        const auto data = tftp::to_buffer(data_packet);
        const auto send_data_error = co_await send_packet(connection_socket, client_endpoint, data);

        if (send_data_error) {
            co_return send_data_error;
        }

        const auto [ack_receive_error, ack_sender_endpoint] =
            co_await receive_packet(connection_socket, receive_buffer, MAX_RECEIVE_WAIT_TIME);

        // No ack to data -> resend data
        if (ack_receive_error == asio::error::timed_out) {
            continue;
        }

        if (ack_receive_error) {
            co_return ack_receive_error;
        }

        // Wrong packet origin -> send error to origin and ignore the packet
        if (ack_sender_endpoint != client_endpoint) {
            co_await send_packet(
                connection_socket, ack_sender_endpoint,
                tftp::to_buffer(tftp::packet::error{.error_code = 5, .error_msg{"Unknown transfer ID"s}}));
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

asio::awaitable<asio::error_code> receive_loop(asio::ip::udp::socket& connection_socket,
                                               const asio::ip::udp::endpoint& client_endpoint,
                                               std::ofstream& file_stream) {

    auto current_block = tftp::packet::block_t{0};
    auto receive_buffer = std::vector<uint8_t>{};
    auto receive_loop_watchdog = tftp::watchdog{connection_socket.get_executor(), MAX_WAIT_TIME};
    auto is_last_received = false;

    while (file_stream.good()) {
        if (receive_loop_watchdog.has_expired()) {
            co_return asio::error::timed_out;
        }

        const auto ack = tftp::to_buffer(tftp::packet::ack{.block = current_block});
        const auto send_ack_error = co_await send_packet(connection_socket, client_endpoint, ack);

        if (send_ack_error) {
            co_return send_ack_error;
        }

        const auto [data_receive_error, data_sender_endpoint] =
            co_await receive_packet(connection_socket, receive_buffer, MAX_RECEIVE_WAIT_TIME);

        // The last data packet was received and client didn't try to resend it after ack -> finished
        if (data_receive_error == asio::error::timed_out && is_last_received) {
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
        if (data_sender_endpoint != client_endpoint) {
            co_await send_packet(
                connection_socket, data_sender_endpoint,
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

        auto data = tftp::get_data(receive_buffer);
        file_stream.write(std::bit_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

        if (data.size() < DATA_CHUNK_SIZE) {
            is_last_received = true;
        }

        ++current_block;
    }

    if (!file_stream.good()) {
        tftp::debug("Error while writing file");
        co_return asio::error::fault;
    }

    co_return asio::error_code{};
}

}  // namespace

namespace tftp {

asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> read_connection(
    asio::ip::udp::endpoint client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer) {

    auto executor = co_await asio::this_coro::executor;
    auto filename = get_filename(*request_buffer);
    auto path = std::filesystem::path(filename).filename();
    auto connection_socket = asio::ip::udp::socket{executor, asio::ip::udp::v4()};

    if (std::filesystem::status(path).type() == std::filesystem::file_type::not_found) {
        auto error_buffer =
            to_buffer(packet::error{.error_code = 1, .error_msg{std::format("File {} not found"sv, path.string())}});
        auto send_error = co_await send_packet(connection_socket, client_endpoint, error_buffer);
        co_return std::pair{client_endpoint, send_error};
    }

    auto file_stream = std::ifstream{path, std::ios::in | std::ios::binary};

    auto send_loop_error = co_await asio::co_spawn(executor, send_loop(connection_socket, client_endpoint, file_stream),
                                                   asio::use_awaitable);

    if (send_loop_error) {
        co_return std::pair{client_endpoint, send_loop_error};
    }

    co_return std::pair{client_endpoint, asio::error_code{}};
}

asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> write_connection(
    asio::ip::udp::endpoint client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer) {

    auto executor = co_await asio::this_coro::executor;
    auto filename = get_filename(*request_buffer);
    auto path = std::filesystem::path(filename).filename();
    auto connection_socket = asio::ip::udp::socket{executor, asio::ip::udp::v4()};

    if (std::filesystem::status(path).type() != std::filesystem::file_type::not_found) {
        auto error_buffer = to_buffer(
            packet::error{.error_code = 6, .error_msg{std::format("File {} already exists"sv, path.string())}});
        auto send_error = co_await send_packet(connection_socket, client_endpoint, error_buffer);
        co_return std::pair{client_endpoint, send_error};
    }

    auto file_stream = std::ofstream{path, std::ios::out | std::ios::binary};

    auto receive_loop_error = co_await asio::co_spawn(
        executor, receive_loop(connection_socket, client_endpoint, file_stream), asio::use_awaitable);

    if (receive_loop_error) {
        co_return std::pair{client_endpoint, receive_loop_error};
    }

    co_return std::pair{client_endpoint, asio::error_code{}};
}

}  // namespace tftp