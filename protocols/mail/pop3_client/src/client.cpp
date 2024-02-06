#include "client.h"
#include "asio.hpp"
#include "config.h"
#include <iostream>
#include <string>
#include <vector>

using std::literals::string_view_literals::operator""sv;

enum class session_state {
    greeting,
    authorization,
    transaction,
    quit,
    error,
};

constexpr auto server_msg = "S: {}"sv;
constexpr auto client_msg = "C: {}"sv;
constexpr auto user_cmd = "USER {}"sv;
constexpr auto pass_cmd = "PASS {}"sv;
constexpr auto list_cmd = "LIST"sv;
constexpr auto quit_cmd = "QUIT"sv;
constexpr auto ok = "+OK"sv;
constexpr auto end_list = "."sv;

template <typename... Args>
void debug(std::format_string<Args...> const& fmt, Args&&... args) {
    if constexpr (verbose) {
        std::cerr << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
    }
}

template <typename socket_t>
std::string read_data(socket_t& socket, asio::error_code& error) {
    asio::streambuf buffer{};
    auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv, error);

    if (error) {
        return {};
    }

    std::string response{buffers_begin(buffer.data()),
                         buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2};
    buffer.consume(read_bytes);
    debug(server_msg, response);

    return response;
}

template <typename socket_t>
std::vector<std::string> read_list(socket_t& socket, asio::error_code& error) {
    std::vector<std::string> list{};
    asio::streambuf buffer{};

    do {
        auto read_bytes = asio::read_until(socket, buffer, "\r\n"sv, error);

        if (error) {
            break;
        }

        list.emplace_back(buffers_begin(buffer.data()),
                          buffers_begin(buffer.data()) + static_cast<std::ptrdiff_t>(read_bytes) - 2);
        debug(server_msg, list.back());
        buffer.consume(read_bytes);
    } while (!error && list.back() != end_list);

    return list;
}

template <typename socket_t>
void write_data(socket_t& socket, std::string_view data, asio::error_code& error) {
    debug(client_msg, data);
    asio::write(socket, asio::buffer(std::format("{}\r\n"sv, data)), error);
}

bool is_ok(std::string const& response) {
    return response.find(ok, 0) != std::string::npos;
}

template <typename socket_t>
void client(socket_t& socket, std::string_view user, std::string_view pass) {
    session_state state = session_state::greeting;
    bool running = true;
    asio::error_code error{};

    while (running) {
        switch (state) {
            case session_state::greeting: {
                auto response = read_data(socket, error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                if (is_ok(response)) {
                    state = session_state::authorization;
                } else {
                    state = session_state::quit;
                }
                break;
            }
            case session_state::authorization: {
                write_data(socket, std::format(user_cmd, user), error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                auto response = read_data(socket, error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                if (!is_ok(response)) {
                    state = session_state::quit;
                    break;
                }

                write_data(socket, std::format(pass_cmd, pass), error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                response = read_data(socket, error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                if (is_ok(response)) {
                    state = session_state::transaction;
                } else {
                    state = session_state::quit;
                }

                break;
            }
            case session_state::transaction: {
                write_data(socket, list_cmd, error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                auto list = read_list(socket, error);

                state = session_state::quit;
                break;
            }
            case session_state::quit: {
                write_data(socket, quit_cmd, error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                auto response = read_data(socket, error);

                if (error) {
                    state = session_state::error;
                    break;
                }

                running = false;
                break;
            }
            case session_state::error: {
                std::cerr << "Error: " << error.message() << '\n';
                running = false;
                break;
            }
        }
    }
}

template void client<asio::ip::tcp::socket>(asio::ip::tcp::socket& socket, std::string_view, std::string_view);
template void client<tls_socket_t>(tls_socket_t& socket, std::string_view, std::string_view);