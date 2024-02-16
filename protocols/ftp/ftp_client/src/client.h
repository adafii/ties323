#pragma once
#include "asio/awaitable.hpp"
#include "asio/ip/tcp.hpp"

namespace ftpc {

/**
 * FTP client
 * @param control_socket FTP control control_socket
 * @return
 */
asio::awaitable<asio::error_code> client(asio::ip::tcp::socket&& control_socket);

}  // namespace ftpc
