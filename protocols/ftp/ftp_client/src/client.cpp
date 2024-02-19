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

constexpr auto connect_timeout = std::chrono::milliseconds{500};
constexpr auto read_timeout = std::chrono::milliseconds{100};

using std::literals::string_view_literals::operator""sv;
using namespace asio::experimental::awaitable_operators;

enum class command {
    ls,
    cat,
    help,
    quit,
    unknown,
};

enum class login_status {
    login_ok,
    login_fail,
};

// clang-format off
const auto input_to_command =  std::vector<std::pair<const std::regex, const command>>{
    {std::regex{R"(^\s*ls\s*$)", std::regex::icase}, command::ls},
    {std::regex{R"(^\s*cat\s+(\S+)\s*$)", std::regex::icase}, command::cat},
    {std::regex{R"(^\s*help\s*$)", std::regex::icase}, command::help},
    {std::regex{R"(^\s*quit\s*$)", std::regex::icase}, command::quit},
    {std::regex{R"(^.*$)", std::regex::icase}, command::unknown}
};
// clang-format on

constexpr auto end_of_line = "\r\n"sv;
constexpr auto ftp_prompt = "ftp> "sv;
constexpr auto file_not_found = "File doesn't exist"sv;

constexpr auto opts_command = "OPTS MLST type;size;modify;UNIX.mode;UNIX.uid;UNIX.gid;"sv;
constexpr auto user_command = "USER {}"sv;
constexpr auto pass_command = "PASS {}"sv;
constexpr auto pasv_command = "PASV"sv;
constexpr auto list_command = "LIST"sv;
constexpr auto retr_command = "RETR {}"sv;

auto get_reply = std::regex{R"(^(\d+) (.*)$)", std::regex::icase};
auto get_passive_port = std::regex{R"(^227 .* \((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\).*$)", std::regex::icase};

using reply_code = uint32_t;
enum class reply : reply_code {
    service_ready = 220,
    closing_data = 226,
    login_ok = 230,
    enter_password = 331,
    user_invalid = 530,
    failed_to_open = 550,
    unknown,
};

std::pair<const reply, const std::smatch> parse_reply(const std::string& reply_string) {
    auto submatch = std::smatch{};
    if (std::regex_match(reply_string, submatch, get_reply)) {
        const auto code = static_cast<reply>(std::stoi(submatch.str(1)));
        return {code, submatch};
    }

    return {reply::unknown, {}};
}

std::pair<const command, const std::smatch> parse_input(const std::string& input) {
    auto submatch = std::smatch{};
    for (auto& [regex, command] : input_to_command) {
        if (std::regex_match(input, submatch, regex)) {
            return {command, submatch};
        }
    }

    return {command::unknown, {}};
}

asio::awaitable<void> write_data(asio::ip::tcp::socket& socket, std::string_view data) {
    ftpc::debug(ftpc::client_debug, data);
    co_await asio::async_write(socket, asio::buffer(std::format("{}{}"sv, data, end_of_line)), asio::use_awaitable);
}

asio::awaitable<std::pair<asio::error_code, std::vector<std::string>>> read_multiple_lines(
    asio::ip::tcp::socket& socket,
    std::chrono::milliseconds timeout) {
    auto lines = std::vector<std::string>{};
    auto buffer = asio::streambuf{};
    const auto executor = co_await asio::this_coro::executor;
    bool read_succeeded = false;
    auto read_error = asio::error_code{};

    do {
        read_error = asio::error_code{};
        auto result = co_await (asio::async_read_until(socket, buffer, end_of_line,
                                                       asio::redirect_error(asio::use_awaitable, read_error)) ||
                                asio::steady_timer{executor, timeout}.async_wait(asio::use_awaitable));

        const auto* read_bytes = get_if<std::size_t>(&result);
        read_succeeded = (!read_error) && (read_bytes != nullptr);
        if (read_succeeded) {
            lines.emplace_back(buffers_begin(buffer.data()),
                               buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(*read_bytes) - 2);
            ftpc::debug(ftpc::server_debug, lines.back());
            buffer.consume(*read_bytes);
        }
    } while (read_succeeded);

    if (lines.empty()) {
        co_return std::pair{asio::error::timed_out, lines};
    }

    co_return std::pair{asio::error_code{}, lines};
}

asio::awaitable<std::pair<asio::error_code, std::string>> read_raw_data(asio::ip::tcp::socket& socket,
                                                                        std::chrono::milliseconds timeout) {
    auto buffer = asio::streambuf{};
    const auto executor = co_await asio::this_coro::executor;
    auto read_error = asio::error_code{};

    read_error = asio::error_code{};
    auto result = co_await (
        asio::async_read(socket, buffer, asio::transfer_all(), asio::redirect_error(asio::use_awaitable, read_error)) ||
        asio::steady_timer{executor, timeout}.async_wait(asio::use_awaitable));

    const auto* read_bytes = get_if<std::size_t>(&result);
    if (read_error != asio::error::eof || read_bytes == nullptr) {
        co_return std::pair{asio::error::timed_out, ""};
    }

    auto data = std::string{};
    std::copy_if(buffers_begin(buffer.data()), buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(*read_bytes),
                 std::back_inserter(data), [](char c) { return c != '\r'; });

    ftpc::debug(ftpc::server_debug, data);

    co_return std::pair{asio::error_code{}, data};
}

asio::awaitable<std::pair<asio::error_code, login_status>> ftp_login(asio::ip::tcp::socket& control_socket,
                                                                     std::string_view user,
                                                                     std::string_view password) {
    const auto [greet_error, greet_reply] = co_await read_multiple_lines(control_socket, read_timeout);

    if (greet_error) {
        co_return std::pair{greet_error, login_status::login_fail};
    }

    [[maybe_unused]] const auto [greet_code, greet_arguments] = parse_reply(greet_reply.back());

    if (greet_code != reply::service_ready) {
        co_return std::pair{asio::error_code{}, login_status::login_fail};
    }

    co_await write_data(control_socket, opts_command);
    co_await read_multiple_lines(control_socket, read_timeout);

    co_await write_data(control_socket, std::format(user_command, user));
    const auto [user_error, user_reply] = co_await read_multiple_lines(control_socket, read_timeout);

    if (user_error) {
        co_return std::pair{user_error, login_status::login_fail};
    }

    [[maybe_unused]] const auto [user_code, user_arguments] = parse_reply(user_reply.back());

    switch (user_code) {
        case reply::enter_password:
            break;
        case reply::login_ok:
            co_return std::pair{asio::error_code{}, login_status::login_ok};
        case reply::user_invalid:
            [[fallthrough]];
        default:
            co_return std::pair{asio::error_code{}, login_status::login_fail};
    }

    co_await write_data(control_socket, std::format(pass_command, password));
    const auto [password_error, password_reply] = co_await read_multiple_lines(control_socket, read_timeout);

    if (password_error) {
        co_return std::pair{password_error, login_status::login_fail};
    }

    [[maybe_unused]] const auto [password_code, password_arguments] = parse_reply(password_reply.back());

    switch (password_code) {
        case reply::login_ok:
            break;
        default:
            co_return std::pair{asio::error_code{}, login_status::login_fail};
    }

    co_return std::pair{asio::error_code{}, login_status::login_ok};
}

asio::awaitable<std::tuple<asio::error_code, std::string, std::string>> ftp_passive_mode(
    asio::ip::tcp::socket& control_socket) {
    co_await write_data(control_socket, pasv_command);
    const auto [error, reply] = co_await read_multiple_lines(control_socket, read_timeout);

    if (error) {
        co_return std::tuple{error, "", ""};
    }

    auto submatch = std::smatch{};
    if (!std::regex_match(reply.back(), submatch, get_passive_port)) {
        co_return std::tuple{asio::error::fault, "", ""};
    }

    const auto host = std::format("{}.{}.{}.{}"sv, submatch.str(1), submatch.str(2), submatch.str(3), submatch.str(4));
    const auto port_hex = std::format("{:0>2x}{:0>2x}"sv, std::stoi(submatch.str(5)), std::stoi(submatch.str(6)));
    const auto port = std::format("{:d}"sv, std::stoi(port_hex, nullptr, 16));

    co_return std::tuple{asio::error_code{}, host, port};
}

asio::awaitable<asio::error_code> execute(asio::ip::tcp::socket& control_socket, std::string_view command) {
    const auto [passive_error, data_host, data_port] = co_await ftp_passive_mode(control_socket);

    if (passive_error) {
        co_return passive_error;
    }

    auto [connect_error, data_socket] = co_await ftpc::connect_with_timeout(data_host, data_port, connect_timeout);

    if (connect_error) {
        co_return connect_error;
    }

    co_await write_data(control_socket, command);

    const auto [control_error, control_lines] = co_await read_multiple_lines(control_socket, read_timeout);

    if (control_error) {
        co_return control_error;
    }

    [[maybe_unused]] const auto [control_code, control_arguments] = parse_reply(control_lines.back());

    switch (control_code) {
        case reply::failed_to_open: {
            std::cout << file_not_found << '\n';
            data_socket.close();
            co_return asio::error_code{};
        }
        case reply::closing_data: {
            break;
        }
        default: {
            co_return asio::error_code{};
        }
    }

    auto [error, data] = co_await read_raw_data(data_socket, read_timeout);

    if (error) {
        co_return error;
    }

    std::cout << data;

    if (!data.ends_with('\n')) {
        std::cout << '\n';
    }

    data_socket.close();
    co_return asio::error_code{};
}

}  // namespace

asio::awaitable<asio::error_code> ftpc::client(asio::ip::tcp::socket&& control_socket,
                                               std::string_view user,
                                               std::string_view password) {
    auto running = true;

    const auto [login_error, login_status] = co_await ftp_login(control_socket, user, password);

    if (login_error || login_status != login_status::login_ok) {
        co_return login_error;
    }

    while (running) {
        auto input = std::string{};
        std::cout << ftp_prompt;
        std::getline(std::cin, input);
        const auto [command, arguments] = parse_input(input);

        switch (command) {
            case command::ls: {
                auto error = co_await execute(control_socket, list_command);
                if (error) {
                    co_return error;
                }
                break;
            }
            case command::cat: {
                auto error = co_await execute(control_socket, std::format(retr_command, arguments.str(1)));
                if (error) {
                    co_return error;
                }
                break;
            }
            case command::quit: {
                running = false;
                break;
            }
            case command::help: {
                [[fallthrough]];
            }
            default: {
                std::cout << "Commands:\n"
                          << "ls - list remote files\n"
                          << "cat - output remote file to stdout\n"
                          << "help - this help\n"
                          << "quit - disconnect and quit\n";
                break;
            }
        }
    }

    co_return asio::error_code{};
}