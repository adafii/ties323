#pragma once
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <vector>

namespace tftp {

namespace packet {

enum class opcode : std::uint16_t {
    rrq = 1,
    wrq = 2,
    data = 3,
    ack = 4,
    error = 5,
};

struct [[maybe_unused]] rrq {
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
    std::uint16_t block{};
    std::vector<std::uint8_t> data{};
};

struct ack {
    opcode op{opcode::ack};
    std::uint16_t block{};
};

struct [[maybe_unused]] error {
    opcode op{opcode::error};
    std::uint16_t error_code{};
    std::vector<std::uint8_t> error_msg{};
};

}  // namespace packet

// Helpers to break packets into bytes

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
    std::ranges::copy(array | std::views::reverse,  // Reverse = little endian -> big endian
                      std::back_inserter(buffer));
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

inline std::vector<uint8_t> to_buffer(const packet::wrq&& packet) {
    return make_buffer<uint8_t>(packet.op, packet.filename, packet.mode);
}

inline std::vector<uint8_t> to_buffer(const packet::data&& packet) {
    return make_buffer<uint8_t>(packet.op, packet.block, packet.data);
}

template <byteish T, std::size_t size>
inline packet::opcode get_opcode(const std::array<T, size>& buffer) {
    static_assert(size / sizeof(T) >= 2, "Buffer too small, fix your code!");
    const auto opcode_array = std::array<T, sizeof(packet::opcode) / sizeof(T)>{buffer[1], buffer[2]};
    return std::bit_cast<packet::opcode>(opcode_array);
}

/* FIXME template <byteish T, std::size_t size>
inline packet::ack to_ack(const std::array<T, size>& buffer) {
    return std::bit_cast<packet::ack>(std::span(buffer | std::views::take(sizeof(packet::ack))));
}*/

template <typename... Args>
inline void debug(const std::format_string<Args...>& fmt, Args&&... args) {
    std::cerr << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

inline void debug_packet(std::string_view message, const byteish_range auto& buffer, std::size_t data_size) {
    std::cerr << message;

    for (const auto& byte : buffer | std::views::take(data_size)) {
        if(std::isprint(byte)) {
            std::cerr << std::format("{:<2c} ", byte);
            continue;
        }
        std::cerr << std::format("{:0>2x} ", byte);
    }
    std::cerr << "\n";
}

}  // namespace tftp