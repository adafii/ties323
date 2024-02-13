#include "smtp.h"
#include "asio.hpp"
#include "config.h"
#include <format>
#include <iostream>
#include <ranges>
#include <regex>
#include <string_view>
#include <unordered_map>

using std::literals::string_view_literals::operator""sv;

namespace {

enum class session_state {
    greet,
    wait_helo,
    init_transaction,
    recipients,
    data,
    quit,
};

enum class command {
    helo,
    mail,
    rcpt,
    data,
    quit,
    unknown,
};

using reply_code = uint32_t;
const auto replies = std::unordered_map<reply_code, const std::string_view>{
    {211, "{} System status, or system help make_reply"sv},
    {214, "{} Help"sv},
    {220, "{} {} Simple Mail Transfer Service ready"sv},
    {221, "{} {} Service closing transmission channel"sv},
    {250, "{} {}"sv},
    {251, "{} User not local; will forward to {}"sv},
    {354, "{} Start recipients input; end with <CRLF>.<CRLF>"sv},
    {421, "{} {} Service not available, closing transmission channel"sv},
    {450, "{} Requested recipients action not taken: mailbox unavailable"sv},
    {451, "{} Requested action aborted: local error in processing"sv},
    {452, "{} Requested action not taken: insufficient system storage"sv},
    {500, "{} Syntax error, command unrecognized"sv},
    {501, "{} Syntax error in parameters or arguments"sv},
    {502, "{} Command not implemented"sv},
    {503, "{} Bad sequence of commands"sv},
    {504, "{} Command parameter not implemented"sv},
    {550, "{} Requested action not taken: mailbox unavailable"sv},
    {551, "{} User not local; please try {}"sv},
    {552, "{} Requested recipients action aborted: exceeded storage allocation"sv},
    {553, "{} Requested action not taken: mailbox name not allowed"sv},
    {554, "{} Transaction failed"sv},
};

constexpr auto ok = "OK"sv;
constexpr auto end_of_data = "."sv;

// clang-format off
const auto request_parser = std::vector<std::pair<const std::regex, const command>>{
    {std::regex{R"-(^helo (?:((?:(?:[a-z][a-z0-9\-]*[a-z0-9]+)\.)*(?:[a-z][a-z0-9\-]*[a-z0-9]+)+)|(.+))$)-", std::regex::icase}, command::helo},
    {std::regex{R"-(^mail (?:(?:from:<((?:(?:(?:[a-z0-9]+)\.)*(?:[a-z0-9]+)+)@(?:(?:(?:[a-z][a-z0-9\-]*[a-z0-9]+)\.)*(?:[a-z][a-z0-9\-]*[a-z0-9]+)+))>)|(.+))$)-", std::regex::icase}, command::mail},
    {std::regex{R"-(^rcpt (?:(?:to:<((?:(?:(?:[a-z0-9]+)\.)*(?:[a-z0-9]+)+)@(?:(?:(?:[a-z][a-z0-9\-]*[a-z0-9]+)\.)*(?:[a-z][a-z0-9\-]*[a-z0-9]+)+))>)|(.+))$)-", std::regex::icase}, command::rcpt},
    {std::regex{R"-(^data$)-", std::regex::icase}, command::data},
    {std::regex{R"-(^quit$)-", std::regex::icase}, command::quit},
    {std::regex{R"-(^.*$)-", std::regex::icase}, command::unknown},
};
// clang-format on

const auto is_local_regexp = std::regex{std::format("@{}", host_name), std::regex::icase};
constexpr auto is_local = [](const auto& recipient) { return std::regex_search(recipient, is_local_regexp); };
constexpr auto is_invalid = [](const auto& arguments) { return !arguments.str(arguments.size() - 1).empty(); };

template <typename... Args>
std::string make_reply(reply_code code, Args... args) {
    if (code == 250 && sizeof...(args) == 0) {
        return make_reply(250, ok);
    }

    return std::vformat(replies.at(code), std::make_format_args(code, args...));
}

std::pair<command, std::smatch> parse_request(std::string const& response) {
    std::smatch submatch{};

    for (const auto& [regex, command] : request_parser) {
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

asio::awaitable<std::vector<std::string>> read_multiple_lines(asio::ip::tcp::socket& socket) {
    auto lines = std::vector<std::string>{};
    auto buffer = asio::streambuf{};

    do {
        auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv);

        lines.emplace_back(buffers_begin(buffer.data()),
                           buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2);
        buffer.consume(read_bytes);
    } while (lines.back() != end_of_data);

    co_return lines;
}

}  // namespace

asio::awaitable<void> smtp_session(asio::ip::tcp::socket socket, std::shared_ptr<mail_storage> storage) {
    auto state = session_state::greet;
    auto running = true;
    auto current_mail = mail{};

    try {
        while (running) {
            switch (state) {
                case session_state::greet: {
                    co_await write_data(socket, make_reply(220, host_name));
                    state = session_state::wait_helo;
                    break;
                }
                case session_state::wait_helo: {
                    const auto request = co_await read_data(socket);
                    const auto [command, arguments] = parse_request(request);

                    switch (command) {
                        case command::helo:
                            if (is_invalid(arguments)) {
                                co_await write_data(socket, make_reply(501));
                                break;
                            }

                            co_await write_data(socket, make_reply(250, host_name));
                            state = session_state::init_transaction;
                            break;
                        case command::mail:
                            [[fallthrough]];
                        case command::rcpt:
                            [[fallthrough]];
                        case command::data:
                            co_await write_data(socket, make_reply(503));
                            break;
                        case command::quit:
                            state = session_state::quit;
                            break;
                        default:
                            co_await write_data(socket, make_reply(500));
                            break;
                    }
                    break;
                }
                case session_state::init_transaction: {
                    const auto request = co_await read_data(socket);
                    const auto [command, arguments] = parse_request(request);

                    current_mail = mail{};

                    switch (command) {
                        case command::helo:
                            if (is_invalid(arguments)) {
                                co_await write_data(socket, make_reply(501));
                                break;
                            }

                            co_await write_data(socket, make_reply(250, host_name));
                            break;
                        case command::mail:
                            if (is_invalid(arguments)) {
                                co_await write_data(socket, make_reply(501));
                                break;
                            }

                            co_await write_data(socket, make_reply(250));
                            current_mail.from = arguments.str(1);
                            state = session_state::recipients;
                            break;
                        case command::rcpt:
                            [[fallthrough]];
                        case command::data:
                            co_await write_data(socket, make_reply(503));
                            break;
                        case command::quit:
                            state = session_state::quit;
                            break;
                        default:
                            co_await write_data(socket, make_reply(500));
                            break;
                    }
                    break;
                }
                case session_state::recipients: {
                    const auto request = co_await read_data(socket);
                    const auto [command, arguments] = parse_request(request);

                    switch (command) {
                        case command::rcpt:
                            if (is_invalid(arguments)) {
                                co_await write_data(socket, make_reply(501));
                                break;
                            }

                            co_await write_data(socket, make_reply(250));
                            current_mail.recipients.insert(arguments.str(1));
                            break;
                        case command::data:
                            if (current_mail.recipients.empty()) {
                                co_await write_data(socket, make_reply(503));
                                break;
                            }

                            state = session_state::data;
                            break;
                        case command::helo:
                            [[fallthrough]];
                        case command::mail:
                            co_await write_data(socket, make_reply(503));
                            break;
                        case command::quit:
                            state = session_state::quit;
                            break;
                        default:
                            co_await write_data(socket, make_reply(500));
                            break;
                    }
                    break;
                }
                case session_state::data: {
                    co_await write_data(socket, make_reply(354));
                    auto data = co_await read_multiple_lines(socket);

                    current_mail.message = data;

                    {
                        const auto lock = std::lock_guard<std::mutex>{storage->write_lock};

                        for (const auto& recipient : current_mail.recipients | std::views::filter(is_local)) {
                            storage->maildrops[recipient].mails["INBOX"].push_back(current_mail);
                        }
                    }

                    co_await write_data(socket, make_reply(250));

                    state = session_state::init_transaction;
                    break;
                }
                case session_state::quit: {
                    co_await write_data(socket, make_reply(221, host_name));
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