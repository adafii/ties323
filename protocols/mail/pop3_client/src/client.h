#pragma once
#include <string_view>
#include "tls_socket.h"
#include "asio/awaitable.hpp"

/**
 * @brief POP3 client
 * @param socket Open asio socket to host
 * @param user User
 * @param pass Password
 */
void client(tls_socket& socket, std::string_view user, std::string_view pass);