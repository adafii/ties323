#pragma once
#include "asio/awaitable.hpp"
#include "tls_socket.h"
#include <string_view>

/**
 * @brief POP3 client
 * @param socket Open asio socket to host
 * @param user User
 * @param pass Password
 */
template <typename socket_t>
extern void client(socket_t& socket, std::string_view user, std::string_view pass);