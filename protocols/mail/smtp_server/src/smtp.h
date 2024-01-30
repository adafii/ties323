#pragma once
#include <asio/ip/tcp.hpp>
#include <asio/awaitable.hpp>

/**
 * @brief SMTP session manager
 * @param socket TCP socket for the session
 */
asio::awaitable<void> smtp_session(asio::ip::tcp::socket socket);