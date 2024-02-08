#include "imap.h"
#include "asio.hpp"
#include "config.h"
#include <iostream>
#include <regex>
#include <string_view>

using std::literals::string_view_literals::operator""sv;

namespace {

constexpr auto greeting = "* OK {} IMAP4 Server"sv;
constexpr auto no = "{} NO {}"sv;

enum class session_state {
    greeting,
    unauthenticated,
    authenticated,
    logout,
    error,
};

enum class command {
    login,
    tag_unknown,
    unknown,
};

const auto command_parser = std::vector<std::pair<std::regex, command>>{
    {{std::regex(R"-(^(\w+) login (\S+) (\S+)\s\s$)-", std::regex::icase)}, command::login},
    {{std::regex(R"-(^(\w+).*$)-", std::regex::icase)}, command::tag_unknown},
    {{std::regex(R"-(.*)-", std::regex::icase)}, command::unknown},
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
                            user = arguments.str(1);
                            pass = arguments.str(2);

                            if (!storage->maildrops.contains(user)) {
                                // write_data()
                            }

                            break;
                        }
                    }
                }
                case session_state::authenticated: {
                }
                case session_state::logout: {
                }
                case session_state::error: {
                }
            }
        }
    } catch (std::exception& e) {
        std::cerr << "IMAP exception: " << e.what() << '\n';
    }
}