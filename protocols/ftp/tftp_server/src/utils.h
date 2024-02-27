#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <ranges>
#include <algorithm>
#include <iostream>

namespace tftp {

namespace packet {

enum class opcode : std::uint16_t {
    unknown = 0,
    rrq = 1,
    wrq = 2,
    data = 3,
    ack = 4,
    error = 5,
};

using block_t = std::uint16_t;
using error_t = block_t;

struct rrq {
    opcode op{opcode::rrq};
    std::string filename{};
    std::string mode{"octet"};
};

struct wrq {
    opcode op{opcode::wrq};
    std::string filename{};
    std::string mode{"octet"};
};

struct data {
    opcode op{opcode::data};
    block_t block{1};
    std::vector<std::uint8_t> data{};
};

struct ack {
    opcode op{opcode::ack};
    block_t block{1};
};

struct [[maybe_unused]] error {
    opcode op{opcode::error};
    error_t error_code{};
    std::string error_msg{};
};

}

// Helpers to break packets into bytes before sending

template <typename T>
concept byteish = sizeof(T) == sizeof(std::uint8_t);

template <typename T>
concept stringish = std::ranges::range<T> && (std::same_as<T, std::string> || std::same_as<T, std::string_view>);

template <typename T>
concept integralish = std::integral<T> || std::integral<std::underlying_type_t<T>>;

template <typename T>
concept byteish_range = std::ranges::range<T> && byteish<std::ranges::range_value_t<T>> && !stringish<T>;

template <byteish T>
inline void copy(std::vector<T>& buffer, const byteish_range auto& range) {
    std::ranges::copy(range, std::back_inserter(buffer));
}

template <byteish T>
inline void copy(std::vector<T>& buffer, integralish auto integral) {
    auto array = std::bit_cast<std::array<T, sizeof(integral)>>(integral);
    std::ranges::copy(array | std::views::reverse, std::back_inserter(buffer));  // Little endian -> big endian
}

template <byteish T>
inline void copy(std::vector<T>& buffer, const stringish auto& string) {
    std::ranges::copy(string, std::back_inserter(buffer));
    buffer.push_back('\0');
}

template <byteish T, typename... Args>
inline std::vector<T> make_buffer(Args&&... args) {
    auto buffer = std::vector<T>{};
    (copy(buffer, std::forward<Args>(args)), ...);
    return buffer;
}

inline std::vector<std::uint8_t> to_buffer(const packet::data& packet) {
    return make_buffer<std::uint8_t>(packet.op, packet.block, packet.data);
}

inline std::vector<std::uint8_t> to_buffer(const packet::ack& packet) {
    return make_buffer<std::uint8_t>(packet.op, packet.block);
}

inline std::vector<std::uint8_t> to_buffer(const packet::error& packet) {
    return make_buffer<std::uint8_t>(packet.op, packet.error_code, packet.error_msg);
}

inline packet::opcode get_opcode(const byteish_range auto& buffer) {
    if (buffer.size() < sizeof(packet::opcode)) {
        return packet::opcode::unknown;
    }

    const auto opcode_array = std::array<std::uint8_t, sizeof(packet::opcode)>{buffer[1], buffer[0]};
    const auto opcode = std::bit_cast<std::uint16_t>(opcode_array);

    if (opcode > static_cast<std::uint16_t>(packet::opcode::error)) {
        return packet::opcode::unknown;
    }

    return static_cast<packet::opcode>(opcode);
}

// Debug helpers

template <typename... Args>
inline void debug(const std::format_string<Args...>& fmt, Args&&... args) {
    std::cerr << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

inline void debug_packet(std::string_view message, const byteish_range auto& buffer) {
    std::cerr << message;

    const auto opcode = get_opcode(buffer);

    if (opcode == packet::opcode::data) {
        for (const auto& byte : buffer | std::views::take(4)) {
            std::cerr << std::format("{:0>2x} ", byte);
        }
        std::cerr << std::format("[ {} bytes of data ]\n", buffer.size() - 4);
        return;
    }

    for (const auto& byte : buffer | std::views::take(buffer.size())) {
        if (opcode != packet::opcode::ack && std::isprint(byte)) {
            std::cerr << std::format("{:<2c} ", byte);
            continue;
        }
        std::cerr << std::format("{:0>2x} ", byte);
    }
    std::cerr << "\n";
}

}  // namespace tftp