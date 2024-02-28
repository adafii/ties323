#pragma once
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <vector>

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

}  // namespace packet

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

inline std::vector<std::uint8_t> to_buffer(const packet::rrq& packet) {
    return make_buffer<std::uint8_t>(packet.op, packet.filename, packet.mode);
}

inline std::vector<std::uint8_t> to_buffer(const packet::wrq& packet) {
    return make_buffer<std::uint8_t>(packet.op, packet.filename, packet.mode);
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

/**
 * Get opcode from data or ack packet buffer
 * @param buffer Received bytes
 * @return Opcode
 */
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

/**
 * Get block number from data or ack packet buffer
 * @param buffer Received bytes
 * @return Block number
 */
inline packet::block_t get_block_number(const byteish_range auto& buffer) {
    if (buffer.size() < sizeof(packet::opcode) + sizeof(packet::block_t)) {
        return 0;
    }

    const auto block_array = std::array<std::uint8_t, sizeof(packet::block_t)>{buffer[3], buffer[2]};
    return std::bit_cast<packet::block_t>(block_array);
}

/**
 * Get opcode, error number and error message from error packet
 * @param buffer Received bytes
 * @return Opcode, error number and error message
 */
inline std::tuple<packet::opcode, packet::error_t, std::string> get_error(const byteish_range auto& buffer) {
    if (buffer.size() <= sizeof(packet::opcode) + sizeof(packet::error_t) + 1) {
        return {packet::opcode::unknown, 0, {}};
    }

    const auto opcode = tftp::get_opcode(buffer);
    const auto error_code = tftp::get_block_number(buffer);

    if (opcode != packet::opcode::error) {
        return {opcode, 0, {}};
    }

    const auto error_message = std::string{buffer.begin() + sizeof(packet::opcode) + sizeof(packet::error_t), buffer.end() - 1};

    return {opcode, error_code, error_message};
}

/**
 * Get data from data packet
 * @param buffer Received bytes
 * @return Span of data bytes. UB if the lifespan of buffer ends before the return value.
 */
inline std::span<const std::uint8_t> get_data(const byteish_range auto& buffer) {
    if (buffer.size() <= sizeof(packet::opcode) + sizeof(packet::block_t)) {
        return {};
    }

    return {buffer.begin() + sizeof(packet::opcode) + sizeof(packet::block_t), buffer.end()};
}

/**
 * Watchdog emits cancellation signal if the timer is not reset before the timeout interval
 */
class watchdog {
public:
    /**
     * Construct watchdog without starting it
     * @param executor Coro executor
     */
    explicit watchdog(const asio::any_io_executor& executor) : timer_{executor} {}

    /**
     * Construct watchdog and start the timer
     * @param executor Coro executor
     * @param timeout_interval Watchdog should be reset before the timeout
     */
    watchdog(const asio::any_io_executor& executor, std::chrono::milliseconds timeout_interval) : watchdog{executor} {
        this->start(timeout_interval);
    }

    /**
     * Start watchdog timer
     * @param timeout_interval Watchdog should be reset before the timeout
     */
    void start(std::chrono::milliseconds timeout_interval) {
        timeout_interval_ = timeout_interval;
        reset();
    }

    /**
     * Reset watchdog timer. This expires immediately if called before start().
     */
    void reset() {
        timer_.expires_after(timeout_interval_);
        timer_.async_wait([this](asio::error_code error) {
            if (!error) {
                has_expired_.test_and_set();
                time_out_signal_.emit(asio::cancellation_type::terminal);
            }
        });
    };

    /**
     * Checks if the timer has expired
     * @return True, if the timer has expired
     */
    [[nodiscard]] bool has_expired() const {
        return has_expired_.test();
    };

    /**
     * Get slot that receives cancel signals from watchdog timeouts
     * @return Cancel signal slot
     */
    [[nodiscard]] asio::cancellation_slot get_signal_slot() { return time_out_signal_.slot(); }

private:
    asio::steady_timer timer_;
    asio::cancellation_signal time_out_signal_{};
    asio::chrono::milliseconds timeout_interval_{0};
    std::atomic_flag has_expired_{false};
};

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

        std::cerr << std::format("[ {} bytes of data ]", buffer.size() - 4);

        if ( buffer.size() < 512 ) {
            std::cerr << " (last)";
        }

        std::cerr << '\n';
        return;
    }

    for (const auto& byte : buffer) {
        if (opcode != packet::opcode::ack && std::isprint(byte)) {
            std::cerr << std::format("{:<2c} ", byte);
            continue;
        }
        std::cerr << std::format("{:0>2x} ", byte);
    }
    std::cerr << "\n";
}

}  // namespace tftp