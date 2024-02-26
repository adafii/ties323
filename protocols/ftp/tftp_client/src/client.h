#pragma once
#include "asio/awaitable.hpp"
#include "asio/error_code.hpp"
#include "asio/ip/udp.hpp"
#include "asio/stream_file.hpp"
#include <string_view>
#include <filesystem>

namespace tftp {

/**
 * Client for writing files to remote server
 * @param socket UDP socket
 * @param source Source file
 * @param server Remote server
 * @return Error?
 */
asio::awaitable<asio::error_code> write_client(asio::ip::udp::socket&& socket,
                                               std::filesystem::path&& source,
                                               asio::ip::udp::endpoint server);

/**
 * Client for reading files from remote server
 * @param socket UDP socket
 * @param target Target file
 * @param server Remote server
 * @return Error?
 */
asio::awaitable<asio::error_code> read_client(asio::ip::udp::socket&& socket,
                                              std::filesystem::path&& target,
                                              asio::ip::udp::endpoint server);

}  // namespace tftp