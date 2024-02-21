#pragma once
#include <algorithm>
#include <cstdint>
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
    std::uint16_t block{};
    std::vector<uint8_t> data{};
};

struct ack {
    opcode op{opcode::ack};
    std::uint16_t block{};
};

struct error {
    opcode op{opcode::error};
    std::uint16_t error_code{};
    std::string error_msg{};
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
    std::ranges::copy(array | std::views::reverse, std::back_inserter(buffer));
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

inline std::vector<uint8_t> to_buffer(const packet::wrq& packet) {
    return make_buffer<uint8_t>(packet.op, packet.filename, packet.mode);
}

}  // namespace tftp