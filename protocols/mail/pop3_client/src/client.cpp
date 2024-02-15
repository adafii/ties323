#include "client.h"
#include "asio.hpp"
#include "asio/ssl.hpp"
#include "config.h"
#include "mail.h"
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using std::literals::string_view_literals::operator""sv;
using std::literals::string_literals::operator""s;

namespace {

enum class session_state {
    greeting,
    authorization,
    transaction,
    quit,
};

constexpr auto server_msg = "S: {}"sv;
constexpr auto client_msg = "C: {}"sv;
constexpr auto user_cmd = "USER {}"sv;
constexpr auto pass_cmd = "PASS {}"sv;
constexpr auto list_cmd = "LIST"sv;
constexpr auto retr_cmd = "RETR {}"sv;
constexpr auto quit_cmd = "QUIT"sv;
constexpr auto ok = "+OK"sv;
constexpr auto end_list = "."sv;

const auto message_number = std::regex(R"(^(\d+) \d+$)", std::regex::icase);
const auto header = std::regex(R"(^([^:]+): (.+)$)", std::regex::icase);

bool is_ok(const std::string& response) {
    return response.find(ok, 0) != std::string::npos;
}

mail parse_message(const std::vector<std::string>& message) {
    auto new_mail = mail{};
    uint32_t body_start = 0;
    auto submatch = std::smatch{};

    for (std::size_t line = 0; line < message.size(); ++line) {
        if (std::regex_match(message[line], submatch, header)) {
            new_mail.headers[submatch.str(1)] = submatch.str(2);
        }

        if (message[line].empty()) {
            body_start = line + 1;
            break;
        }
    }

    for (std::size_t line = body_start; line < message.size() - 1; ++line) {
        new_mail.body += message[line] + '\n';
    }

    return new_mail;
}

template <typename... Args>
void debug(const std::format_string<Args...>& fmt, Args&&... args) {
    if constexpr (verbose) {
        std::cerr << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
    }
}

template <typename socket_t>
std::string read_data(socket_t& socket) {
    auto buffer = asio::streambuf{};
    auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv);

    auto response = std::string{buffers_begin(buffer.data()),
                                buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2};
    buffer.consume(read_bytes);
    debug(server_msg, response);

    return response;
}

template <typename socket_t>
std::vector<std::string> read_multiline(socket_t& socket) {
    auto list = std::vector<std::string>{};
    auto buffer = asio::streambuf{};

    do {
        auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv);

        list.emplace_back(buffers_begin(buffer.data()),
                          buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2);
        debug(server_msg, list.back());
        buffer.consume(read_bytes);
    } while (list.back() != end_list);

    return list;
}

template <typename socket_t>
void write_data(socket_t& socket, std::string_view data) {
    debug(client_msg, data);
    asio::write(socket, asio::buffer(std::format("{}\r\n"sv, data)));
}

template <typename socket_t>
void client(socket_t& socket, std::string_view user, std::string_view pass, mail_storage& storage) {
    auto state = session_state::greeting;
    auto running = true;

    while (running) {
        switch (state) {
            case session_state::greeting: {
                auto response = read_data(socket);

                if (is_ok(response)) {
                    state = session_state::authorization;
                } else {
                    state = session_state::quit;
                }
                break;
            }
            case session_state::authorization: {
                write_data(socket, std::format(user_cmd, user));
                auto response = read_data(socket);

                if (!is_ok(response)) {
                    state = session_state::quit;
                    break;
                }

                write_data(socket, std::format(pass_cmd, pass));
                response = read_data(socket);

                if (is_ok(response)) {
                    state = session_state::transaction;
                } else {
                    state = session_state::quit;
                }

                break;
            }
            case session_state::transaction: {
                write_data(socket, list_cmd);
                auto list = read_multiline(socket);

                auto message_numbers = std::vector<uint32_t>{};
                auto submatch = std::smatch{};

                for (const auto& str : list) {
                    if (std::regex_match(str, submatch, message_number)) {
                        message_numbers.emplace_back(std::stoi(submatch.str(1)));
                    }
                }

                for (const auto& number : message_numbers) {
                    write_data(socket, std::format(retr_cmd, number));
                    auto message = read_multiline(socket);
                    storage.emplace_back(parse_message(message));
                }

                state = session_state::quit;
                break;
            }
            case session_state::quit: {
                write_data(socket, quit_cmd);
                auto response = read_data(socket);
                running = false;
                break;
            }
        }
    }
}

}  // namespace

std::string pop3_get_mail(const std::string& username,
                   const std::string& password,
                   const std::string& host,
                   const std::string& port,
                   mail_storage& storage) {
    try {
        auto io_context = asio::io_context{1};

        auto signals = asio::signal_set{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        auto socket = asio::ip::tcp::socket{io_context};
        auto resolver = asio::ip::tcp::resolver{io_context};

        asio::connect(socket, resolver.resolve(host, port));

        client(socket, username, password, storage);
    } catch (std::exception& e) {
        return e.what();
    }

    return "OK"s;
}

std::string pop3_get_mail_tls(const std::string& username,
                       const std::string& password,
                       const std::string& host,
                       const std::string& port,
                       mail_storage& storage) {
    try {
        auto io_context = asio::io_context{1};

        auto signals = asio::signal_set{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        auto tls_context = asio::ssl::context{asio::ssl::context::tlsv13};
        tls_context.set_default_verify_paths();

        auto tls_socket = tls_socket_t{io_context, tls_context};
        auto resolver = asio::ip::tcp::resolver{io_context};

        asio::connect(tls_socket.lowest_layer(), resolver.resolve(host, port));

        tls_socket.set_verify_mode(asio::ssl::verify_peer);
        tls_socket.set_verify_callback(asio::ssl::host_name_verification(std::string(host)));

        tls_socket.handshake(tls_socket_t::client);

        client(tls_socket, username, password, storage);
    } catch (std::exception& e) {
        return e.what();
    }

    return "OK"s;
}