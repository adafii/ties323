#pragma once
#include "asio/ip/tcp.hpp"
#include "asio/ssl/stream.hpp"

using tls_socket_t = asio::ssl::stream<asio::ip::tcp::socket>;