#pragma once
#include "asio/ssl/stream.hpp"
#include "asio/ip/tcp.hpp"

using tls_socket = asio::ssl::stream<asio::ip::tcp::socket>;