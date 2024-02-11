#include "imap.h"
#include "asio.hpp"
#include "config.h"
#include <iostream>
#include <regex>
#include <string_view>

using std::literals::string_view_literals::operator""sv;

namespace {

constexpr auto greeting = "* OK {} IMAP4 Server"sv;
constexpr auto no = "{} NO"sv;
constexpr auto ok = "{} OK"sv;
constexpr auto bad_tag = "{} BAD"sv;
constexpr auto bad = "* BAD"sv;
constexpr auto bye = "* BYE"sv;
constexpr auto list = R"-(* LIST () "{}" "{}")-"sv;

enum class session_state {
    greeting,
    unauthenticated,
    authenticated,
    quit,
};

enum class command {
    login,
    logout,
    list,
    tag_unknown,
    unknown,
};

const auto command_parser = std::vector<std::pair<std::regex, command>>{
    {{std::regex(R"-(^(\w+) login (\S+) (\S+)$)-", std::regex::icase)}, command::login},
    {{std::regex(R"-(^(\w+) list (?:"([ \.\/\-\*_\w]*)"|([\.\/\-\*_\w]+)) (?:"([ \.\/\-\*_\w]+)"|([\.\/\-\*_\w]+))$)-",
                 std::regex::icase)},
     command::list},
    {{std::regex(R"-(^(\w+) logout$)-", std::regex::icase)}, command::logout},
    {{std::regex(R"-(^(\w+).*$)-", std::regex::icase)}, command::tag_unknown},
    {{std::regex(R"-(^.*$)-", std::regex::icase)}, command::unknown},
};

std::pair<command, std::smatch> parse_request(std::string const& response) {
    std::smatch submatch{};

    for (auto const& [regex, command] : command_parser) {
        if (std::regex_match(response, submatch, regex)) {
            return {command, submatch};
        }
    }

    return {command::unknown, submatch};
}

void write_data(asio::ip::tcp::socket& socket, std::string_view data) {
    asio::write(socket, asio::buffer(std::format("{}\r\n"sv, data)));
}

std::string read_data(asio::ip::tcp::socket& socket) {
    auto buffer = asio::streambuf{};
    auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv);

    auto response = std::string{buffers_begin(buffer.data()),
                                buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2};
    buffer.consume(read_bytes);

    return response;
}

}  // namespace

asio::awaitable<void> imap_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage) {
    auto state = session_state::greeting;
    auto running = true;
    auto user = std::string{};
    auto pass = std::string{};

    try {
        while (running) {
            switch (state) {
                case session_state::greeting: {
                    write_data(socket, std::format(greeting, host_name));
                    state = session_state::unauthenticated;
                    break;
                }
                case session_state::unauthenticated: {
                    auto request = read_data(socket);
                    auto const [command, arguments] = parse_request(request);

                    switch (command) {
                        case command::login: {
                            auto tag = arguments.str(1);
                            auto tried_user = arguments.str(2);
                            [[maybe_unused]] auto tried_pass = arguments.str(3);

                            if (tried_user.find('@') == std::string::npos) {
                                tried_user.append(std::format("@{}", host_name));
                            }

                            if (!storage->maildrops.contains(tried_user)) {
                                write_data(socket, std::format(no, tag));
                                break;
                            }

                            // Skip password check

                            user = tried_user;
                            pass = tried_pass;

                            write_data(socket, std::format(ok, tag));
                            state = session_state::authenticated;
                            break;
                        }
                        case command::logout: {
                            auto tag = arguments.str(1);
                            write_data(socket, bye);
                            write_data(socket, std::format(ok, tag));
                            state = session_state::quit;
                            break;
                        }
                        case command::tag_unknown: {
                            auto tag = arguments.str(1);
                            write_data(socket, std::format(bad_tag, tag));
                            break;
                        }
                        default: {
                            write_data(socket, bad);
                            break;
                        }
                    }
                    break;
                }
                case session_state::authenticated: {
                    auto request = read_data(socket);
                    auto const [command, arguments] = parse_request(request);

                    switch (command) {
                        case command::list: {
                            auto tag = arguments.str(1);
                            auto reference = arguments.str(2).empty() ? arguments.str(3) : arguments.str(2);
                            auto mailbox_name = arguments.str(4).empty() ? arguments.str(5) : arguments.str(4);

                            if (!(reference.empty() || reference == "/")) {
                                write_data(socket, std::format(ok, tag));
                                break;
                            }

                            // Only supports * or <mailbox name>, because true globbing would be too much for this demo

                            if (mailbox_name == "*") {
                                for (const auto& [mailbox, mails] : storage->maildrops.at(user).mails) {
                                    write_data(socket, std::format(list, "/", mailbox));
                                }
                            } else if (storage->maildrops.at(user).mails.contains(mailbox_name)) {
                                write_data(socket, std::format(list, "/", mailbox_name));
                            }

                            write_data(socket, std::format(ok, tag));
                            break;
                        }
                        case command::logout: {
                            auto tag = arguments.str(1);
                            write_data(socket, bye);
                            write_data(socket, std::format(ok, tag));
                            state = session_state::quit;
                            break;
                        }
                        case command::tag_unknown: {
                            auto tag = arguments.str(1);
                            write_data(socket, std::format(bad_tag, tag));
                            break;
                        }
                        default: {
                            write_data(socket, bad);
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
        std::cerr << "IMAP exception: " << e.what() << '\n';
    }

    co_return;
}