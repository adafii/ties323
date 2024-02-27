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

int main(int argc, char* argv[]) {
    auto args = std::vector<std::string_view>{argv + 1, argv + argc};

    auto help = []() {
        std::cout << "Usage: tftpc ( -r | -w ) file host port\n"
                  << "    -r Read file from host (RRQ)\n"
                  << "    -w Write file to host (WRQ)\n";
    };

    if (args.size() != 4) {
        help();
        return EXIT_FAILURE;
    }

    auto mode = args[0];
    auto filename = args[1];
    auto host = args[2];
    auto port = args[3];

    if (!(mode == "-r" || mode == "-w")) {
        help();
        return EXIT_FAILURE;
    }

    auto path = fs::path(filename).lexically_normal();

    if (mode == "-r" && std::filesystem::status(path).type() != std::filesystem::file_type::not_found) {
        std::cerr << std::format("File {} exists already!\n", path.string());
        return EXIT_FAILURE;
    }

    if (mode == "-w" && std::filesystem::status(path).type() == std::filesystem::file_type::not_found) {
        std::cerr << std::format("File {} not found!\n", path.string());
        return EXIT_FAILURE;
    }

    try {
        auto io_context = asio::io_context{};

        auto signals = asio::signal_set{io_context, SIGINT, SIGTERM};
        signals.async_wait([&io_context](auto, auto) { io_context.stop(); });

        auto socket = asio::ip::udp::socket{io_context, asio::ip::udp::v4()};
        auto resolver = asio::ip::udp::resolver(io_context);
        auto server = *resolver.resolve(asio::ip::udp::v4(), host, port).begin();

        auto client_error = std::atomic_flag{false};

        auto handler = [&io_context, &client_error](auto exception, const auto error) {
            if (exception) {
                std::rethrow_exception(exception);
            }

            if (error) {
                std::cerr << std::format("Client returned error: {}\n", error.message());
                client_error.test_and_set();
            }

            io_context.stop();
        };

        if (mode == "-w") {
            asio::co_spawn(io_context, tftp::write_client(std::move(socket), std::move(path), server), handler);
        } else {
            asio::co_spawn(io_context, tftp::read_client(std::move(socket), std::move(path), server), handler);
        }

        io_context.run();

        if (client_error.test()) {
            return EXIT_FAILURE;
        }
    } catch (std::exception& exception) {
        std::cerr << std::format("Exception: {}\n", exception.what());
    }

    return EXIT_SUCCESS;
}
