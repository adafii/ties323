#include "asio.hpp"
#include "config.h"
#include "mail.h"
#include "pop3.h"
#include "smtp.h"

asio::awaitable<void> listener(
    asio::ip::tcp::acceptor&& acceptor,
    std::function<asio::awaitable<void>(asio::ip::tcp::socket, std::shared_ptr<mail_storage>)> session_manager,
    std::shared_ptr<mail_storage> storage) {
    for (;;) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        co_spawn(socket.get_executor(), session_manager(std::move(socket), storage), asio::detached);
    }
}

int main() {
    auto storage = std::make_shared<mail_storage>();

    // Mock data
    storage->maildrops["liia@localhost"].mails.push_back(
        {.from{"test@localhost"},
         .recipients{"test@localhost"},
         .message{
             "To: liia@localhost\r\nFrom: test@localhost\r\nSubject: Greetings\r\n\r\nHello, from SMTP\r\n.\r\n"}});

    storage->maildrops["test@localhost"];

    try {
        asio::io_context io_context{10};

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        auto smtp_socket_acceptor = asio::ip::tcp::acceptor{io_context, {asio::ip::tcp::v4(), smtp_port}};
        co_spawn(io_context, listener(std::move(smtp_socket_acceptor), smtp_session, storage), asio::detached);

        auto pop3_socket_acceptor = asio::ip::tcp::acceptor{io_context, {asio::ip::tcp::v4(), pop3_port}};
        co_spawn(io_context, listener(std::move(pop3_socket_acceptor), pop3_session, storage), asio::detached);

        io_context.run();
    } catch (std::exception& e) {
        std::printf("Exception: %s\n", e.what());
    }
}