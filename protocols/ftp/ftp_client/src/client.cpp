#include "client.h"
#include "asio.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "utils.h"
#include <chrono>
#include <iostream>
#include <ranges>
#include <regex>
#include <string_view>
#include <vector>

namespace {

using std::literals::string_view_literals::operator""sv;
using namespace asio::experimental::awaitable_operators;

enum class command {
    ls,
    cat,
    quit,
    unknown,
};

// clang-format off
const auto input_to_command =  std::vector<std::pair<const std::regex, const command>>{
    {std::regex{R"(^\s*ls\s*$)", std::regex::icase}, command::ls},
    {std::regex{R"(^\s*cat\s+(\S+)\s*$)", std::regex::icase}, command::cat},
    {std::regex{R"(^\s*quit\s*$)", std::regex::icase}, command::quit},
    {std::regex{R"(^.*$)", std::regex::icase}, command::unknown}
};
// clang-format on

std::pair<const command, const std::smatch> parse_input(const std::string& input) {
    auto submatch = std::smatch{};
    for (auto& [regex, command] : input_to_command) {
        if (std::regex_match(input, submatch, regex)) {
            return {command, submatch};
        }
    }

    return {command::unknown, {}};
};

asio::awaitable<void> write_data(asio::ip::tcp::socket& socket, std::string_view data) {
    co_await asio::async_write(socket, asio::buffer(std::format("{}\r\n"sv, data)), asio::use_awaitable);
}

asio::awaitable<std::string> read_data(asio::ip::tcp::socket& socket) {
    auto buffer = asio::streambuf{};
    auto response = std::string{};

    auto executor = co_await asio::this_coro::executor;
    auto result = co_await (asio::async_read_until(socket, buffer, "\r\n"sv, asio::use_awaitable) ||
                            asio::steady_timer{executor, ftpc::read_wait}.async_wait(asio::use_awaitable));

    if (const auto* read_bytes = get_if<std::size_t>(&result); read_bytes != nullptr) {
        response = std::string{buffers_begin(buffer.data()),
                               buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(*read_bytes) - 2};
        buffer.consume(*read_bytes);
    }

    co_return response;
}

}  // namespace

asio::awaitable<asio::error_code> ftpc::client(asio::ip::tcp::socket&& control_socket) {
    auto input = std::string{};
    auto running = true;

    while (running) {
        std::cout << "ftp> ";
        std::getline(std::cin, input);

        auto [command, arguments] = parse_input(input);

        switch (command) {
            case command::ls: {
                co_await write_data(control_socket, "LIST"sv);

                auto [error, data_socket] = co_await connect_with_timeout("localhost"sv, "2222"sv);

                if (error) {
                    co_return error;
                }

                auto data = co_await read_data(data_socket);
                std::cout << data << '\n';

                break;
            }
            case command::quit: {
                running = false;
                break;
            }
            default: {
                std::cout << "Commands: ls cat quit\n";
                break;
            }
        }
    }

    co_return asio::error_code{};
}
