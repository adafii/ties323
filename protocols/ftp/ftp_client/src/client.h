#pragma once
#include <string_view>
#include "asio/awaitable.hpp"
#include "asio/ip/tcp.hpp"

namespace ftpc {

/**
 * FTP client
 * @param control_socket FTP control control_socket
 * @return Exit status
 */
asio::awaitable<asio::error_code> client(asio::ip::tcp::socket&& control_socket,
                                         std::string_view user,
                                         std::string_view password);

}  // namespace ftpc
