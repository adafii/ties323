#include "smtp.h"
#include <algorithm>
#include <asio.hpp>
#include <format>
#include <iostream>
#include <string_view>
#include <unordered_map>

using std::literals::string_view_literals::operator""sv;

constexpr auto host_name = "SMTP.LOCALHOST"sv;

enum class session_state {
    greet,
    init,
    ready,
    mail,
    data,
    quit,
};

enum class command_t {
    helo,
    mail,
    from,
    rcpt,
    data,
    quit,
    none,
};

const std::unordered_map<std::string_view, command_t> command_as_string{
    {"helo", command_t::helo}, {"mail", command_t::mail}, {"from", command_t::from},
    {"rcpt", command_t::rcpt}, {"data", command_t::data}, {"quit", command_t::quit},
};

struct message {
    command_t command{command_t::none};
    uint32_t num_tokens{0};
    std::string from{};
    std::string to{};
    std::string argument{};
};

const std::unordered_map<uint32_t, const std::string_view> replies{
    {211, "{} System status, or system help reply"sv},
    {214, "{} Help message"sv},
    {220, "{} {} Service ready"sv},
    {221, "{} {} Service closing transmission channel"sv},
    {250, "{} OK"sv},
    {251, "{} User not local; will forward to <forward-path>"sv},  // TODO: replace <forward-path>
    {354, "{} Start mail input; end with <CRLF>.<CRLF>"sv},
    {421, "{} {} Service not available, closing transmission channel"sv},
    {450, "{} Requested mail action not taken: mailbox unavailable"sv},
    {451, "{} Requested action aborted: local error in processing"sv},
    {452, "{} Requested action not taken: insufficient system storage"sv},
    {500, "{} Syntax error, command unrecognized"sv},
    {501, "{} Syntax error in parameters or arguments"sv},
    {502, "{} Command not implemented"sv},
    {503, "{} Bad sequence of commands"sv},
    {504, "{} Command parameter not implemented"sv},
    {550, "{} Requested action not taken: mailbox unavailable"sv},
    {551, "{} User not local; please try <forward-path>"sv},  // TODO: replace <forward-path>
    {552, "{} Requested mail action aborted: exceeded storage allocation"sv},
    {553, "{} Requested action not taken: mailbox name not allowed"sv},
    {554, "{} Transaction failed"sv},
};

std::string reply_msg(uint32_t reply_code) {
    if (!replies.contains(reply_code)) {
        return "???";
    }

    return std::vformat(replies.at(reply_code), std::make_format_args(reply_code, host_name)) + '\n';
}

std::string reply_msg(uint32_t reply_code, std::string_view reply_message) {
    return std::format("{} {}", reply_code, reply_message) + '\n';
}

void split(std::string const& str, std::vector<std::string>& tokens, std::string const& delimiter) {
    std::size_t start = str.find_first_not_of(delimiter);

    if (start == std::string::npos) {
        return;
    }

    std::size_t next = str.find_first_of(delimiter, start);
    std::size_t end = str.find_last_not_of(delimiter);

    while (next <= end && next != std::string::npos) {
        tokens.push_back(str.substr(start, next - start));
        start = str.find_first_not_of(delimiter, next);
        next = str.find_first_of(delimiter, start);
    }

    tokens.emplace_back(str.substr(start, next - start));
}

std::pair<bool, uint32_t> execute(message const& message) {
    switch (message.command) {
        case command_t::none:
            return {true, 500};
        case command_t::helo:
            if (message.num_tokens != 2) {
                return {true, 501};
            }
            return {false, 250};
        case command_t::mail:
            if (message.num_tokens != 2 || message.from.empty()) {
                return {true, 501};
            }
            return {false, 250};
        case command_t::rcpt:
            if (message.num_tokens != 2 || message.to.empty()) {
                return {true, 501};
            }
            return {false, 250};
        case command_t::data:
            if (message.num_tokens != 1) {
                return {true, 501};
            }
            return {false, 354};
        case command_t::quit:
            if (message.num_tokens != 1) {
                return {true, 500};
            }
            return {false, 221};
        default:
            return {true, 500};
    }
}

message parse_tokens(std::vector<std::string> const& tokens) {
    message parsed_message{};

    if (tokens.empty() || tokens.size() > 2) {
        return parsed_message;
    }

    parsed_message.num_tokens = tokens.size();

    std::string const& command = tokens.front();
    std::string compare_command{};
    std::transform(command.begin(), command.end(), std::back_inserter(compare_command),
                   [](auto chr) { return std::tolower(chr); });

    if (command_as_string.contains(compare_command)) {
        parsed_message.command = command_as_string.at(compare_command);
    }

    if (tokens.size() == 2) {
        std::vector<std::string> argument_tokens{};
        split(tokens.at(1), argument_tokens, ":");

        if (argument_tokens.size() == 2) {
            std::string from_to_compare{};
            std::string const& from_to_token = argument_tokens.front();
            std::transform(from_to_token.begin(), from_to_token.end(), std::back_inserter(from_to_compare),
                           [](auto chr) { return std::tolower(chr); });

            if (from_to_compare == "to") {
                parsed_message.to = argument_tokens.at(1);
            } else if (from_to_compare == "from") {
                parsed_message.from = argument_tokens.at(1);
            }
        } else {
            parsed_message.argument = tokens.at(1);
        }
    }

    return parsed_message;
}

asio::awaitable<message> receive_from_socket(asio::ip::tcp::socket& socket) {
    std::string data{};
    co_await asio::async_read_until(socket, asio::dynamic_buffer(data, 1024), "\r\n", asio::use_awaitable);

    std::vector<std::string> received_tokens{};
    split(data, received_tokens, " \r\n");

    co_return parse_tokens(received_tokens);
}

asio::awaitable<void> smtp_session(asio::ip::tcp::socket socket) {
    try {
        auto state = session_state::greet;
        bool running = true;

        auto receive_message = [&socket]() -> asio::awaitable<message> { return receive_from_socket(socket); };

        auto send_reply = [&socket](uint32_t reply_code) -> asio::awaitable<void> {
            co_await asio::async_write(socket, asio::buffer(reply_msg(reply_code)), asio::use_awaitable);
        };

        auto send_reply_msg = [&socket](uint32_t reply_code, std::string_view reply_message) -> asio::awaitable<void> {
            co_await asio::async_write(socket, asio::buffer(reply_msg(reply_code, reply_message)), asio::use_awaitable);
        };

        while (running) {
            switch (state) {
                case session_state::greet: {
                    co_await send_reply(220);
                    state = session_state::init;
                    break;
                }
                case session_state::init: {
                    auto const message = co_await receive_message();
                    auto const [error, reply_code] = execute(message);

                    if (error) {
                        co_await send_reply(reply_code);
                        state = session_state::init;
                        break;
                    }

                    switch (message.command) {
                        case command_t::helo:
                            co_await send_reply_msg(reply_code, host_name);
                            state = session_state::ready;
                            break;
                        case command_t::quit:
                            co_await send_reply(reply_code);
                            state = session_state::quit;
                            break;
                        default:
                            co_await send_reply(503);
                            state = session_state::init;
                            break;
                    }
                    break;
                }
                case session_state::ready: {
                    auto const message = co_await receive_message();
                    auto const [error, reply_code] = execute(message);

                    if (error) {
                        co_await send_reply(reply_code);
                        state = session_state::ready;
                        break;
                    }

                    switch (message.command) {
                        case command_t::helo:
                            co_await send_reply_msg(reply_code, host_name);
                            state = session_state::ready;
                            break;
                        case command_t::mail:
                            co_await send_reply(reply_code);
                            state = session_state::mail;
                            break;
                        case command_t::quit:
                            co_await send_reply(reply_code);
                            state = session_state::quit;
                            break;
                        default:
                            co_await send_reply(503);
                            state = session_state::ready;
                            break;
                    }
                    break;
                }
                case session_state::mail: {
                    auto const message = co_await receive_message();
                    auto const [error, reply_code] = execute(message);

                    if (error) {
                        co_await send_reply(reply_code);
                        state = session_state::mail;
                        break;
                    }

                    switch (message.command) {
                        case command_t::helo:
                            co_await send_reply_msg(reply_code, host_name);
                            state = session_state::mail;
                            break;
                        case command_t::rcpt:
                            co_await send_reply(reply_code);
                            state = session_state::mail;
                            break;
                        case command_t::data:
                            co_await send_reply(reply_code);
                            state = session_state::data;
                            break;
                        case command_t::quit:
                            co_await send_reply(reply_code);
                            state = session_state::quit;
                            break;
                        default:
                            co_await send_reply(503);
                            state = session_state::mail;
                            break;
                    }
                    break;
                }
                case session_state::data: {
                    for (;;) {
                        std::string data{};
                        co_await asio::async_read_until(socket, asio::dynamic_buffer(data, 1024), "\r\n",
                                                        asio::use_awaitable);

                        std::cout << data.size() << '\n';
                        if (data == ".\r\n") {
                            state = session_state::ready;
                            break;
                        }
                    }

                    co_await send_reply(250);

                    break;
                }
                case session_state::quit: {
                    socket.close();
                    running = false;
                    break;
                }
            }
        }
    } catch (std::exception& e) {
        std::printf("SMTP exception: %s\n", e.what());
    }
}