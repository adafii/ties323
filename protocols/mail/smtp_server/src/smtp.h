#pragma once
#include "mail.h"
#include "asio/awaitable.hpp"
#include "asio/ip/tcp.hpp"

/**
 * @brief SMTP session manager
 * @param socket TCP socket for the session
 * @param storage Mail storage
 */
asio::awaitable<void> smtp_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage);