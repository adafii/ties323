#include "pop3.h"
#include <iostream>

enum class session_state {
    greeting,
    authorization,
    transaction,
    quit,
};

asio::awaitable<void> pop3_session(asio::ip::tcp::socket /*socket*/, std::shared_ptr<mail_storage> /*storage*/) {
    session_state state = session_state::greeting;
    bool running = true;

    try {
        while (running) {
            switch (state) {
                case session_state::greeting: {
                    state = session_state::quit;
                    break;
                }
                case session_state::authorization: {
                    break;
                }
                case session_state::transaction: {
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