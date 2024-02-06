#include "pop3.h"
#include "asio.hpp"
#include "config.h"
#include <iostream>
#include <numeric>
#include <regex>
#include <string_view>

using std::literals::string_view_literals::operator""sv;

constexpr auto negative = "-ERR"sv;
constexpr auto no_user = "-ERR no such user here"sv;
constexpr auto already_locked = "-ERR mail-drop already locked"sv;
constexpr auto positive = "+OK"sv;
constexpr auto list_ok = "+OK {} messages ({} octets)"sv;
constexpr auto greeting = "+OK POP3 server ready"sv;
constexpr auto quitting = "+OK POP3 server signing off"sv;
constexpr auto list_entry = "{} {}"sv;
constexpr auto list_end = "."sv;

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

std::vector<std::pair<std::regex, command>> command_parser{
    {std::regex{R"(^user ([a-z0-9]+\.)*[a-z0-9]+(@([a-z0-9]+\.)*[a-z0-9]+)?$)", std::regex::icase}, command::user},
    {std::regex{R"(^pass \S+$)", std::regex::icase}, command::pass},
    {std::regex{R"(^list( [0-9]*)?$)", std::regex::icase}, command::list},
    {std::regex{R"(^quit$)", std::regex::icase}, command::quit},
};

command parse_command(std::string const& response) {
    for (auto const& [regex, command] : command_parser) {
        if (std::regex_match(response, regex)) {
            return command;
        }
    }

    return command::unknown;
}

std::string get_argument(std::string const& response) {
    auto space = response.find_first_of(' ');

    if (space == std::string::npos) {
        return {};
    }

    return response.substr(space + 1);
}

asio::awaitable<void> write_data(asio::ip::tcp::socket& socket, std::string_view data) {
    co_await asio::async_write(socket, asio::buffer(std::format("{}\r\n"sv, data)), asio::use_awaitable);
}

asio::awaitable<std::string> read_data(asio::ip::tcp::socket& socket) {
    asio::streambuf buffer{};
    auto read_bytes = co_await asio::async_read_until(socket, buffer, "\r\n"sv, asio::use_awaitable);

    std::string response{buffers_begin(buffer.data()),
                         buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2};
    buffer.consume(read_bytes);

    co_return response;
}

asio::awaitable<void> pop3_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage) {
    session_state state = session_state::greeting;
    bool running = true;

    std::string user{};
    std::string pass{};
    std::unique_lock<std::mutex> current_transaction{};

    try {
        while (running) {
            switch (state) {
                case session_state::greeting: {
                    co_await write_data(socket, greeting);
                    state = session_state::authorization;
                    break;
                }
                case session_state::authorization: {
                    auto response = co_await read_data(socket);
                    auto command = parse_command(response);

                    switch (command) {
                        case command::user: {
                            if (!user.empty()) {
                                co_await write_data(socket, negative);
                                break;
                            }

                            auto tried_user = get_argument(response);

                            if (tried_user.find('@') == std::string::npos) {
                                tried_user.append(std::format("@{}", host_name));
                            }

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

                            std::unique_lock try_transaction{storage->maildrops.at(user).transaction, std::defer_lock};
                            auto has_transaction = try_transaction.try_lock();

                            if (!has_transaction) {
                                co_await write_data(socket, already_locked);
                                break;
                            }

                            co_await write_data(socket, positive);
                            pass = get_argument(response);
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
                    auto response = co_await read_data(socket);
                    auto command = parse_command(response);

                    switch (command) {
                        case command::list: {
                            auto const& mails = storage->maildrops[user].mails;
                            auto mail_num = mails.size();
                            auto octets =
                                std::accumulate(mails.begin(), mails.end(), 0,
                                                [](auto acc, auto const& mail) { return acc + mail.message.size(); });

                            co_await write_data(socket, std::format(list_ok, mail_num, octets));

                            for (std::size_t i = 0; i < mails.size(); ++i) {
                                co_await write_data(socket, std::format(list_entry, i + 1, mails[i].message.size()));
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