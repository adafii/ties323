#pragma once
#include "asio.hpp"
#include "mailbox.h"
#include <string_view>

/**
 * @brief POP3 client
 * @param socket Open asio socket to host
 * @param user User
 * @param pass Password
 * @param storage Storage for user's mailboxes
 */
void client(asio::ip::tcp::socket& socket, std::string_view user, std::string_view pass, mailbox_storage& storage);