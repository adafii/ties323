#pragma once
#include "asio.hpp"

namespace tftp {

asio::awaitable<asio::error_code> spawn_connections(asio::ip::udp::socket&& server_socket);

asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> read_connection(
    asio::ip::udp::socket&& connection_socket,
    asio::ip::udp::endpoint&& client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer);

asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> write_connection(
    asio::ip::udp::socket&& connection_socket,
    asio::ip::udp::endpoint&& client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer);

}  // namespace tftp