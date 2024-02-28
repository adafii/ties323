#pragma once
#include "asio.hpp"

namespace tftp {

/**
 * Send file from server to client (RRQ)
 * @param client_endpoint Client address and port
 * @param request_buffer Request the client send as a whole
 * @return Client endpoint and possible error
 */
asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> read_connection(
    asio::ip::udp::endpoint client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer);

/**
 * Receive file from client (RRQ)
 * @param client_endpoint Client address and port
 * @param request_buffer Request the client send as a whole
 * @return Client endpoint and possible error
 */
asio::awaitable<std::pair<asio::ip::udp::endpoint, asio::error_code>> write_connection(
    asio::ip::udp::endpoint client_endpoint,
    std::unique_ptr<std::vector<std::uint8_t>>&& request_buffer);

}  // namespace tftp