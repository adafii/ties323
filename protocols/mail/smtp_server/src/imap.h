#pragma once
#include "asio/awaitable.hpp"
#include "asio/ip/tcp.hpp"
#include "mail.h"
#include <unordered_set>

/**
 * @brief IMAP session manager
 * @param socket TCP socket for the session
 * @param storage Mail storage
 */
asio::awaitable<void> imap_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage);