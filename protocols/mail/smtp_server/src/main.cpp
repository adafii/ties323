#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>

using asio::ip::tcp;
using std::literals::string_view_literals::operator""sv;

constexpr auto greetings = "220 smtp.example Simple Mail Transfer Service Ready\n"sv;
constexpr auto goodbye = "221 smtp.example Service closing transmission channel\n"sv;

enum class session_state {
    greet,
    wait,
    execute,
    quit,
};

void split(std::string const& str, std::vector<std::string>& tokens) {
    std::size_t start = str.find_first_not_of(" \r\n");
    std::size_t next = str.find_first_of(" \r\n", start);
    std::size_t end = str.find_last_not_of(" \r\n");

    while (next <= end && next != std::string::npos) {
        tokens.push_back(str.substr(start, next - start));
        start = str.find_first_not_of(" \r\n", next);
        next = str.find_first_of(" \r\n", start);
    }

    tokens.emplace_back(str.substr(start, next - start));
}

asio::awaitable<void> smtp_session(tcp::socket socket) {
    try {
        std::vector<std::string> command{};
        session_state state{session_state::greet};
        bool running = true;

        while (running) {
            switch (state) {
                case session_state::greet: {
                    co_await asio::async_write(socket, asio::buffer(greetings), asio::use_awaitable);
                    state = session_state::wait;
                    break;
                }
                case session_state::wait: {
                    std::string data{};
                    co_await asio::async_read_until(socket, asio::dynamic_buffer(data, 1024), "\n",
                                                    asio::use_awaitable);
                    split(data, command);
                    state = session_state::execute;
                    break;
                }
                case session_state::execute: {
                    if (command.front() == "QUIT") {
                        state = session_state::quit;
                        break;
                    }

                    command.clear();
                    state = session_state::wait;
                    break;
                }
                case session_state::quit: {
                    co_await asio::async_write(socket, asio::buffer(goodbye), asio::use_awaitable);
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

asio::awaitable<void> listener(tcp::acceptor acceptor) {
    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
        co_spawn(socket.get_executor(), smtp_session(std::move(socket)), asio::detached);
    }
}

int main() {
    try {
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        co_spawn(io_context, listener(tcp::acceptor{io_context, {tcp::v4(), 2525}}), asio::detached);

        io_context.run();
    } catch (std::exception& e) {
        std::printf("Exception: %s\n", e.what());
    }
}