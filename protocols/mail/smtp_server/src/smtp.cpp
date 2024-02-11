#include "smtp.h"
#include "asio.hpp"
#include "config.h"
#include <algorithm>
#include <format>
#include <iostream>
#include <regex>
#include <string_view>
#include <unordered_map>

using std::literals::string_view_literals::operator""sv;

namespace {
const auto domain_regex = std::regex{R"(([a-z0-9]+\.)*[a-z0-9]+)", std::regex::icase};
const auto address_regex = std::regex{R"(<([a-z0-9]+\.)*[a-z0-9]+@([a-z0-9]+\.)*[a-z0-9]+>)", std::regex::icase};

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

const auto command_as_string = std::unordered_map<std::string_view, command_t>{
    {"helo", command_t::helo}, {"mail", command_t::mail}, {"from", command_t::from},
    {"rcpt", command_t::rcpt}, {"data", command_t::data}, {"quit", command_t::quit},
};

struct parsed_command {
    command_t command{command_t::none};
    uint32_t num_tokens{0};
    std::string from{};
    std::string to{};
    std::string domain{};
};

const auto replies = std::unordered_map<uint32_t, const std::string_view>{
    {211, "{} System status, or system help reply"sv},
    {214, "{} Help"sv},
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
    auto start = str.find_first_not_of(delimiter);

    if (start == std::string::npos) {
        return;
    }

    auto next = str.find_first_of(delimiter, start);
    auto end = str.find_last_not_of(delimiter);

    while (next <= end && next != std::string::npos) {
        tokens.push_back(str.substr(start, next - start));
        start = str.find_first_not_of(delimiter, next);
        next = str.find_first_of(delimiter, start);
    }

    tokens.emplace_back(str.substr(start, next - start));
}

std::pair<bool, uint32_t> execute(parsed_command const& message) {
    switch (message.command) {
        case command_t::none:
            return {true, 500};
        case command_t::helo:
            if (message.num_tokens != 2 || message.domain.empty()) {
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

parsed_command parse_tokens(std::vector<std::string> const& tokens) {
    auto parsed_message = parsed_command{};

    if (tokens.empty() || tokens.size() > 2) {
        return parsed_message;
    }

    parsed_message.num_tokens = tokens.size();

    const auto& command = tokens.front();
    auto compare_command = std::string{};
    std::transform(command.begin(), command.end(), std::back_inserter(compare_command),
                   [](auto chr) { return std::tolower(chr); });

    if (command_as_string.contains(compare_command)) {
        parsed_message.command = command_as_string.at(compare_command);
    }

    if (tokens.size() == 2) {
        auto argument_tokens = std::vector<std::string>{};
        split(tokens.at(1), argument_tokens, ":");

        if (argument_tokens.size() == 2) {
            auto from_to_compare = std::string{};
            auto const& from_to_token = argument_tokens.front();

            std::transform(from_to_token.begin(), from_to_token.end(), std::back_inserter(from_to_compare),
                           [](auto chr) { return std::tolower(chr); });

            auto address_token = argument_tokens.at(1);
            auto is_address = regex_match(address_token, address_regex);

            if (from_to_compare == "to" && is_address) {
                parsed_message.to = address_token.substr(1, address_token.size() - 2);
            } else if (from_to_compare == "from" && is_address) {
                parsed_message.from = address_token.substr(1, address_token.size() - 2);
            }

        } else if (regex_match(tokens.at(1), domain_regex)) {
            parsed_message.domain = tokens.at(1);
        }
    }

    return parsed_message;
}

asio::awaitable<parsed_command> receive_command_from_socket(asio::ip::tcp::socket& socket) {
    auto data = std::string{};
    co_await asio::async_read_until(socket, asio::dynamic_buffer(data, 1024), "\r\n", asio::use_awaitable);

    auto received_tokens = std::vector<std::string>{};
    split(data, received_tokens, " \r\n");

    co_return parse_tokens(received_tokens);
}

}  // namespace

asio::awaitable<void> smtp_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage) {
    auto state = session_state::greet;
    auto running = true;

    auto current_mail = mail{};

    auto receive_command = [&socket]() -> asio::awaitable<parsed_command> {
        return receive_command_from_socket(socket);
    };

    auto send_reply = [&socket](uint32_t reply_code) -> asio::awaitable<void> {
        co_await asio::async_write(socket, asio::buffer(reply_msg(reply_code)), asio::use_awaitable);
    };

    auto send_reply_msg = [&socket](uint32_t reply_code, std::string_view reply_message) -> asio::awaitable<void> {
        co_await asio::async_write(socket, asio::buffer(reply_msg(reply_code, reply_message)), asio::use_awaitable);
    };

    try {
        while (running) {
            switch (state) {
                case session_state::greet: {
                    co_await send_reply(220);
                    state = session_state::init;
                    break;
                }
                case session_state::init: {
                    auto const command = co_await receive_command();
                    auto const [error, reply_code] = execute(command);

                    if (error) {
                        co_await send_reply(reply_code);
                        state = session_state::init;
                        break;
                    }

                    switch (command.command) {
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
                    current_mail = mail{};

                    auto const command = co_await receive_command();
                    auto const [error, reply_code] = execute(command);

                    if (error) {
                        co_await send_reply(reply_code);
                        state = session_state::ready;
                        break;
                    }

                    switch (command.command) {
                        case command_t::helo:
                            co_await send_reply_msg(reply_code, host_name);
                            state = session_state::ready;
                            break;
                        case command_t::mail:
                            current_mail.from = command.from;

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
                    auto const command = co_await receive_command();
                    auto const [error, reply_code] = execute(command);

                    if (error) {
                        co_await send_reply(reply_code);
                        state = session_state::mail;
                        break;
                    }

                    switch (command.command) {
                        case command_t::helo:
                            co_await send_reply_msg(reply_code, host_name);
                            state = session_state::mail;
                            break;
                        case command_t::rcpt:
                            current_mail.recipients.insert(command.to);

                            co_await send_reply(reply_code);
                            state = session_state::mail;
                            break;
                        case command_t::data:
                            if (current_mail.recipients.empty()) {
                                co_await send_reply(503);
                                state = session_state::mail;
                                break;
                            }

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
                    auto data = std::string{};
                    auto line = std::string{};
                    auto buffer = asio::streambuf{};
                    auto error = asio::error_code{};

                    do {
                        auto read_bytes =
                            co_await asio::async_read_until(socket, buffer, "\r\n"sv, asio::use_awaitable);

                        if (error) {
                            break;
                        }

                        line = std::string{buffers_begin(buffer.data()),
                                           buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes)};
                        data.append(line);

                        buffer.consume(read_bytes);
                    } while (line != ".\r\n");

                    current_mail.message = data;

                    {
                        const auto lock = std::lock_guard<std::mutex>{storage->write_lock};

                        for (auto const& recipient : current_mail.recipients) {
                            storage->maildrops[recipient].mails["INBOX"].push_back(current_mail);
                        }
                    }

                    co_await send_reply(250);

                    state = session_state::ready;
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
        std::cerr << "SMTP exception: " << e.what() << '\n';
    }
}