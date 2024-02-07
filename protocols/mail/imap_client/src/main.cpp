#include "asio.hpp"
#include "client.h"
#include "mailbox.h"
#include <iostream>

namespace {

void print_mails(mailbox_storage const& storage) {
    constexpr std::string_view columns = "      {:35} {:35} {:40} {:35}";
    for (auto const& [mailbox_name, mailbox] : storage) {
        auto mail_amount = mailbox.size();
        std::cout << std::format("{} ({} {})", mailbox_name, mail_amount, mail_amount == 1 ? "mail" : "mails") << '\n';
        if (mail_amount > 0) {
            std::cout << std::format(columns, "From", "To", "Date", "Subject") << '\n';
        }
        for (auto const& mail : mailbox) {
            std::cout << std::format(columns, mail.from, mail.to, mail.date, mail.subject) << '\n';
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> args{argv + 1, argv + argc};

    if (args.size() != 4) {
        std::cerr << "Usage: " << argv[0] << " <user> <pass> <host> <port>\n";
        return EXIT_FAILURE;
    }

    std::string_view user = args[0];
    std::string_view pass = args[1];
    std::string_view host = args[2];
    std::string_view port = args[3];

    mailbox_storage storage;

    try {
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        asio::ip::tcp::socket socket(io_context);
        asio::ip::tcp::resolver resolver(io_context);

        asio::error_code connection_error{};
        asio::connect(socket, resolver.resolve(host, port), connection_error);

        if (connection_error) {
            std::cerr << "Connection error: " << connection_error.message() << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Connected to " << host << ":" << port << '\n';
        client(socket, user, pass, storage);

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
    }

    print_mails(storage);

    return EXIT_SUCCESS;
}