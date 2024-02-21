#pragma once
#include "asio/awaitable.hpp"
#include "asio/error_code.hpp"
#include "asio/ip/udp.hpp"
#include "asio/stream_file.hpp"
#include <string_view>
#include <filesystem>

namespace tftp {

asio::awaitable<asio::error_code> write_client(asio::ip::udp::socket&& socket,
                                               std::filesystem::path&& source,
                                               asio::ip::udp::endpoint server);

asio::awaitable<asio::error_code> read_client(asio::ip::udp::socket&& socket,
                                              std::filesystem::path&& target,
                                              asio::ip::udp::endpoint server);

}  // namespace tftp