#include "pop3.h"
#include "asio.hpp"
#include "config.h"
#include <iostream>
#include <numeric>
#include <regex>
#include <string_view>

using std::literals::string_view_literals::operator""sv;

namespace {

enum class session_state {
    greeting,
    authorization,
    transaction,
    quit,
};

enum class command {
    user,
    pass,
    list,
    quit,
    unknown,
};

constexpr auto negative = "-ERR"sv;
constexpr auto no_user = "-ERR no such user here"sv;
constexpr auto already_locked = "-ERR mail-drop already locked"sv;
constexpr auto positive = "+OK"sv;
constexpr auto list_ok = "+OK {} messages ({} octets)"sv;
constexpr auto list_one = "+OK {} {}"sv;
constexpr auto greeting = "+OK POP3 server ready"sv;
constexpr auto quitting = "+OK POP3 server signing off"sv;
constexpr auto list_entry = "{} {}"sv;
constexpr auto list_end = "."sv;

const auto get_message_size = [](const std::vector<std::string>& message) {
    return std::accumulate(message.begin(), message.end(), 0ul,
                           [](auto acc, const auto& line) { return acc + line.size(); });
};

const auto command_parser = std::vector<std::pair<std::regex, command>>{
    {std::regex{R"(^user ((?:[a-z][a-z0-9]*\.)*(?:[a-z][a-z0-9]*)@(?:[a-z][a-z0-9]*\.)*(?:[a-z][a-z0-9]*))$)", std::regex::icase}, command::user},
    {std::regex{R"(^pass \S+$)", std::regex::icase}, command::pass},
    {std::regex{R"(^list(?: ([0-9]+))?$)", std::regex::icase}, command::list},
    {std::regex{R"(^quit$)", std::regex::icase}, command::quit},
};

std::pair<command, std::smatch> parse_request(std::string const& response) {
    std::smatch submatch{};

    for (const auto& [regex, command] : command_parser) {
        if (std::regex_match(response, submatch, regex)) {
            return {command, submatch};
        }
    }

    return {command::unknown, submatch};
}

asio::awaitable<void> write_data(asio::ip::tcp::socket& socket, std::string_view data) {
    co_await asio::async_write(socket, asio::buffer(std::format("{}\r\n"sv, data)), asio::use_awaitable);
}

asio::awaitable<std::string> read_data(asio::ip::tcp::socket& socket) {
    auto buffer = asio::streambuf{};
    auto read_bytes = co_await asio::async_read_until(socket, buffer, "\r\n"sv, asio::use_awaitable);

    auto response = std::string{buffers_begin(buffer.data()),
                                buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2};
    buffer.consume(read_bytes);

    co_return response;
}

}  // namespace

asio::awaitable<void> pop3_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage) {
    auto state = session_state::greeting;
    auto running = true;

    auto user = std::string{};
    auto pass = std::string{};
    auto current_transaction = std::unique_lock<std::mutex>{};

    try {
        while (running) {
            switch (state) {
                case session_state::greeting: {
                    co_await write_data(socket, greeting);
                    state = session_state::authorization;
                    break;
                }
                case session_state::authorization: {
                    auto request = co_await read_data(socket);
                    auto [command, arguments] = parse_request(request);

                    switch (command) {
                        case command::user: {
                            if (!user.empty()) {
                                co_await write_data(socket, negative);
                                break;
                            }

                            auto tried_user = arguments.str(1);
                            auto maildrop = storage->maildrops.find(tried_user);

                            if (maildrop == storage->maildrops.end()) {
                                co_await write_data(socket, no_user);
                                break;
                            }

                            user = tried_user;
                            co_await write_data(socket, positive);

                            break;
                        }
                        case command::pass: {
                            if (user.empty()) {
                                co_await write_data(socket, negative);
                                break;
                            }

                            // password check skipped

                            auto try_transaction =
                                std::unique_lock{storage->maildrops.at(user).transaction, std::defer_lock};
                            auto has_transaction = try_transaction.try_lock();

                            if (!has_transaction) {
                                co_await write_data(socket, already_locked);
                                break;
                            }

                            co_await write_data(socket, positive);
                            // pass = get_argument(request);
                            current_transaction.swap(try_transaction);
                            state = session_state::transaction;
                            break;
                        }
                        case command::quit: {
                            co_await write_data(socket, quitting);
                            state = session_state::quit;
                            break;
                        }
                        default: {
                            co_await write_data(socket, negative);
                            break;
                        }
                    }

                    break;
                }
                case session_state::transaction: {
                    auto request = co_await read_data(socket);
                    auto [command, arguments] = parse_request(request);

                    switch (command) {
                        case command::list: {
                            const auto& mails = storage->maildrops[user].mails;
                            auto mail_num = mails.at("INBOX").size();

                            if (!arguments.str(1).empty()) {
                                uint32_t message_num = std::stoi(arguments.str(1));

                                if (message_num < 1 || message_num > static_cast<uint32_t>(mail_num)) {
                                    co_await write_data(socket, negative);
                                    break;
                                }

                                co_await write_data(
                                    socket, std::format(list_one, message_num,
                                                        get_message_size(mails.at("INBOX")[message_num - 1].message)));
                                break;
                            }

                            auto octets = std::accumulate(
                                mails.at("INBOX").begin(), mails.at("INBOX").end(), 0,
                                [](auto acc, const auto& mail) { return acc + get_message_size(mail.message); });

                            co_await write_data(socket, std::format(list_ok, mail_num, octets));

                            for (std::size_t i = 0; i < mails.at("INBOX").size(); ++i) {
                                const auto& message = mails.at("INBOX")[i].message;
                                co_await write_data(socket, std::format(list_entry, i + 1, get_message_size(message)));
                            }

                            co_await write_data(socket, list_end);

                            break;
                        }
                        case command::quit: {
                            co_await write_data(socket, quitting);
                            state = session_state::quit;
                            break;
                        }
                        default: {
                            co_await write_data(socket, negative);
                            break;
                        }
                    }

                    break;
                }
                case session_state::quit: {
                    running = false;
                    break;
                }
            }
        }
    } catch (std::exception& e) {
        std::cerr << "POP3 exception: " << e.what() << '\n';
    }

    co_return;
}