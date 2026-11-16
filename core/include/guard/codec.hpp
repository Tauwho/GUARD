#pragma once

#include "guard/types.hpp"
#include <cstring>
#include <limits>

namespace guard {

enum class DecodeError {
    none,
    bad_message,
    unsupported_version,
    wrong_size,
    schema,
    invalid_value
};

bool valid(const Observation &value);
bool valid(const Proposal &value);
bool valid(const Decision &value);
bool valid(const RegisterReply &value);
bool valid(const Lease &value);
bool valid(const LeaseAck &value);
bool valid(const Revoke &value);
bool valid(const PassthruCmd &value);
bool valid(const RecoverRequest &value);
bool valid(const RecoverReply &value);
bool valid(const MotorCmd &value);
bool valid(const WorldLoad &value);
bool valid(const WorldInject &value);
bool valid(const WorldPause &value);

namespace detail {
template <class T> void clear_reserved(T &) {}
inline void clear_reserved(Proposal &v) { v.based_on.reserved0 = 0; }
inline void clear_reserved(Revoke &v) { v.reserved0 = 0; }
inline void clear_reserved(MotorCmd &v) {
    v.reserved0 = 0;
    v.reserved1 = 0;
}
inline void clear_reserved(Observation &v) { v.reserved0 = 0; }
inline void clear_reserved(Proposal &v) { v.based_on.reserved0 = 0; }
inline void clear_reserved(Decision &v) { v.reserved0 = 0; }
inline void clear_reserved(Lease &v) { v.reserved0 = 0; }
inline void clear_reserved(MotorCmd &v) {
    v.reserved0 = 0;
    v.reserved1 = 0;
}
inline void clear_reserved(WorldLoad &v) {
    v.reserved0 = 0;
    v.reserved1 = 0;
}
inline void clear_reserved(WorldInject &v) { v.reserved0 = {}; }
inline void clear_reserved(WorldPause &v) { v.reserved0 = {}; }
// Walk fields explicitly so encoding never exposes padding or depends on host byte order.
template <bool Reading> struct WireCursor {
    std::conditional_t<Reading, const std::uint8_t *, std::uint8_t *> bytes;
    std::size_t offset = 0;

    template <class T, std::size_t N> void transfer(std::array<T, N> &values) {
        for (auto &value : values) {
            transfer(value);
        }
    }

    template <class T> void transfer(T &value) {
        if constexpr (std::is_arithmetic_v<T>) {
            static_assert(sizeof(T) <= sizeof(std::uint64_t));
            std::uint64_t bits = 0;
            if constexpr (!Reading) {
                if constexpr (std::is_floating_point_v<T>) {
                    static_assert(sizeof(T) == 8 && std::numeric_limits<T>::is_iec559);
                    std::memcpy(&bits, &value, sizeof(T));
                } else {
                    bits = static_cast<std::make_unsigned_t<T>>(value);
                }
            }
            for (std::size_t index = 0; index < sizeof(T); ++index) {
                if constexpr (Reading) {
                    bits |= std::uint64_t(bytes[offset++]) << (index * 8);
                } else {
                    bytes[offset++] = static_cast<std::uint8_t>(bits >> (index * 8));
                }
            }
            if constexpr (Reading) {
                if constexpr (std::is_floating_point_v<T>) {
                    std::memcpy(&value, &bits, sizeof(T));
                } else {
                    using Unsigned = std::make_unsigned_t<T>;
                    const auto narrowed = static_cast<Unsigned>(bits);
                    std::memcpy(&value, &narrowed, sizeof(T));
                }
            }
        } else {
            std::apply([this](auto &...fields) { (transfer(fields), ...); }, value.fields());
        }
    }
};
} // namespace detail

template <class T> DecodeError decode(const std::uint8_t *bytes, std::size_t size, T &output) {
    if (bytes == nullptr || size < sizeof(Header)) {
        return DecodeError::bad_message;
    }
    Header header;
    detail::WireCursor<true> reader{bytes};
    reader.transfer(header);
    if (header.magic != 0x4447 || header.type != T{}.header.type) {
        return DecodeError::bad_message;
    }
    if (header.version != 1) {
        return std::is_same_v<T, Proposal> ? DecodeError::schema : DecodeError::unsupported_version;
    }
    if (size != sizeof(T) || header.size != sizeof(T)) {
        return std::is_same_v<T, Proposal> ? DecodeError::schema : DecodeError::wrong_size;
    }
    T candidate{};
    reader.offset = 0;
    reader.transfer(candidate);
    if (!valid(candidate)) {
        return std::is_same_v<T, Proposal> ? DecodeError::schema : DecodeError::invalid_value;
    }
    output = candidate;
    return DecodeError::none;
}

template <class T> bool encode(T value, std::array<std::uint8_t, sizeof(T)> &output) {
    detail::clear_reserved(value);
    const Header expected = T{}.header;
    if (value.header.magic != expected.magic || value.header.type != expected.type ||
        value.header.version != expected.version || value.header.size != expected.size ||
        !valid(value)) {
        return false;
    }
    std::array<std::uint8_t, sizeof(T)> candidate{};
    detail::WireCursor<false> writer{candidate.data()};
    writer.transfer(value);
    output = candidate;
    return true;
}

} // namespace guard
