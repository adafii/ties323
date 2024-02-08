#include "client.h"
#include "asio.hpp"
#include "config.h"
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using std::literals::string_view_literals::operator""sv;

namespace {

constexpr auto server_msg = "S: {}"sv;
constexpr auto client_msg = "C: {}"sv;
constexpr auto login_command = "{} LOGIN {} {}"sv;
constexpr auto list_command = R"({} LIST "" *)"sv;
constexpr auto examine_command = "{} EXAMINE {}"sv;
constexpr auto logout_command = "{} LOGOUT"sv;
constexpr auto fetch_header_summary = "{} FETCH {} BODY[HEADER.FIELDS (TO FROM DATE SUBJECT)]"sv;

const auto ok_reply = std::regex{R"(^.+ ok.*$)", std::regex::icase};
const auto parse_mailbox = std::regex{R"-(.*LIST.*"(.+)"\s*$)-", std::regex::icase};
const auto has_status = std::regex{R"(^.+ (ok|no|bad|preauth|bye).*$)", std::regex::icase};
const auto parse_exists = std::regex{R"-((\d+) EXISTS)-", std::regex ::icase};
const auto parse_from = std::regex{R"-(from: (.*\S)\s*$)-", std::regex ::icase};
const auto parse_to = std::regex{R"-(to: (.*\S)\s*$)-", std::regex ::icase};
const auto parse_date = std::regex{R"-(date: (.*\S)\s*$)-", std::regex ::icase};
const auto parse_subject = std::regex{R"-(subject: (.*\S)\s*$)-", std::regex ::icase};

enum class session_state {
    greeting,
    unauthenticated,
    authenticated,
    logout,
    error,
};

template <typename... Args>
void debug(std::format_string<Args...> const& fmt, Args&&... args) {
    if constexpr (verbose) {
        std::cerr << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
    }
}

std::string read_data(asio::ip::tcp::socket& socket) {
    auto buffer = asio::streambuf{};
    auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv);

    auto response = std::string{buffers_begin(buffer.data()),
                                buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2};
    buffer.consume(read_bytes);
    debug(server_msg, response);

    return response;
}

std::vector<std::string> read_multiple_lines(asio::ip::tcp::socket& socket) {
    auto lines = std::vector<std::string>{};
    auto buffer = asio::streambuf{};

    do {
        auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv);

        lines.emplace_back(buffers_begin(buffer.data()),
                           buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2);
        debug(server_msg, lines.back());
        buffer.consume(read_bytes);
    } while (!std::regex_match(lines.back(), has_status));

    return lines;
}

void write_data(asio::ip::tcp::socket& socket, std::string_view data) {
    debug(client_msg, data);
    asio::write(socket, asio::buffer(std::format("{}\r\n"sv, data)));
}

bool is_ok(std::string const& response) {
    return std::regex_match(response, ok_reply);
}

std::vector<std::string> extract_mailbox_names(std::vector<std::string> const& response) {
    auto names = std::vector<std::string>{};
    auto submatch = std::smatch{};

    for (auto const& line : response) {
        if (std::regex_match(line, submatch, parse_mailbox)) {
            names.push_back(submatch[1]);
        }
    }

    return names;
}

void store_mailbox_names(std::vector<std::string> const& response, mailbox_storage& storage) {
    auto names = extract_mailbox_names(response);
    for (auto const& name : names) {
        storage.push_back({name, {}});
    }
}

std::uint32_t get_exists_amount(std::vector<std::string> const& response) {
    uint32_t amount = 0;
    auto submatch = std::smatch{};

    for (auto const& line : response) {
        if (std::regex_search(line, submatch, parse_exists)) {
            amount = std::stoi(submatch[1]);
        }
    }

    return amount;
}

void extract_header_summary(std::vector<std::string> const& response, mail& new_mail) {
    auto submatch = std::smatch{};

    for (auto const& line : response) {
        if (std::regex_search(line, submatch, parse_from)) {
            new_mail.from = submatch[1];
        }
        if (std::regex_search(line, submatch, parse_to)) {
            new_mail.to = submatch[1];
        }
        if (std::regex_search(line, submatch, parse_date)) {
            new_mail.date = submatch[1];
        }
        if (std::regex_search(line, submatch, parse_subject)) {
            new_mail.subject = submatch[1];
        }
    }
}

std::vector<mail> fetch_mails(asio::ip::tcp::socket& socket, std::string const& mailbox_name, uint32_t& tag) {
    auto mails = std::vector<mail>{};

    write_data(socket, std::format(examine_command, ++tag, mailbox_name));
    auto response = read_multiple_lines(socket);

    auto exists_amount = get_exists_amount(response);

    for (uint32_t i = 1; i <= exists_amount; ++i) {
        auto new_mail = mail{};

        write_data(socket, std::format(fetch_header_summary, ++tag, i));
        response = read_multiple_lines(socket);

        extract_header_summary(response, new_mail);
        mails.push_back(new_mail);
    }

    return mails;
}

}  // namespace

void client(asio::ip::tcp::socket& socket, std::string_view user, std::string_view pass, mailbox_storage& storage) {
    auto state = session_state::greeting;
    auto running = true;
    std::uint32_t tag = 0;

    try {
        while (running) {
            switch (state) {
                case session_state::greeting: {
                    auto response = read_data(socket);

                    if (!is_ok(response)) {
                        state = session_state::error;
                        break;
                    }

                    state = session_state::unauthenticated;
                    break;
                }
                case session_state::unauthenticated: {
                    write_data(socket, std::format(login_command, ++tag, user, pass));

                    auto response = read_multiple_lines(socket);

                    if (response.empty() || !is_ok(response.back())) {
                        state = session_state::error;
                        break;
                    }

                    state = session_state::authenticated;
                    break;
                }
                case session_state::authenticated: {
                    write_data(socket, std::format(list_command, ++tag));

                    auto response = read_multiple_lines(socket);

                    if (response.empty() || !is_ok(response.back())) {
                        state = session_state::error;
                        break;
                    }

                    store_mailbox_names(response, storage);

                    for (auto& [mailbox_name, mailbox] : storage) {
                        auto mails = fetch_mails(socket, mailbox_name, tag);
                        mailbox = mails;
                    }

                    state = session_state::logout;
                    break;
                }
                case session_state::logout: {
                    write_data(socket, std::format(logout_command, ++tag));
                    read_multiple_lines(socket);
                    running = false;
                    break;
                }
                case session_state::error: {
                    debug("Error, quitting");
                    running = false;
                    break;
                }
            }
        }
    } catch (std::exception& e) {
        std::cerr << "IMAP exception: " << e.what() << '\n';
    }
}
