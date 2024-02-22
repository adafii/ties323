#include "asio.hpp"
#include "asio/stream_file.hpp"
#include "client.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

using std::literals::string_view_literals::operator""sv;
using std::literals::string_literals::operator""s;
namespace fs = std::filesystem;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    auto mode = "-w"sv;
    auto filename = "source.txt"sv;
    auto host = "localhost"sv;
    auto port = "6900"sv;

    if (!(mode == "-r" || mode == "-w")) {
        std::cerr << "help\n";
        return EXIT_FAILURE;
    }

    auto path = fs::path(filename).lexically_normal();

    try {
        auto io_context = asio::io_context{};

        auto signals = asio::signal_set{io_context, SIGINT, SIGTERM};
        signals.async_wait([&io_context](auto, auto) { io_context.stop(); });

        auto socket = asio::ip::udp::socket{io_context, asio::ip::udp::v4()};
        auto resolver = asio::ip::udp::resolver(io_context);
        auto server = *resolver.resolve(asio::ip::udp::v4(), host, port).begin();

        auto handler = [&io_context](auto exception, const auto& error) {
            if (exception) {
                std::rethrow_exception(exception);
            }

            if (error) {
                std::cerr << error.message() << '\n';
            }

            io_context.stop();
        };

        if (mode == "-w") {
            asio::co_spawn(io_context, tftp::write_client(std::move(socket), std::move(path), server), handler);
        } else {
            asio::co_spawn(io_context, tftp::read_client(std::move(socket), std::move(path), server), handler);
        }

        io_context.run();
    } catch (std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }

    return EXIT_SUCCESS;
}
