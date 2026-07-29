#include "atlas/serialization/byte_reader.hpp"

#include <bit>
#include <limits>

namespace atlas::serialization {

// Mirrors the assumption pinned in byte_writer.cpp: read_f32/read_f64 use
// std::bit_cast to reverse write_f32/write_f64, which only round-trips bits
// faithfully if float/double are IEEE-754 binary32/binary64 here too.
static_assert(std::numeric_limits<float>::is_iec559, "atlas-serialization assumes IEEE-754 binary32 float");
static_assert(std::numeric_limits<double>::is_iec559, "atlas-serialization assumes IEEE-754 binary64 double");

std::optional<std::uint64_t> ByteReader::read_unsigned(std::size_t width) noexcept {
    if (remaining() < width) {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(data_[position_ + i])) << (i * 8U);
    }
    position_ += width;
    return value;
}

std::optional<std::uint8_t> ByteReader::read_u8() noexcept {
    const auto value = read_unsigned(sizeof(std::uint8_t));
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(*value);
}

std::optional<std::uint16_t> ByteReader::read_u16() noexcept {
    const auto value = read_unsigned(sizeof(std::uint16_t));
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*value);
}

std::optional<std::uint32_t> ByteReader::read_u32() noexcept {
    const auto value = read_unsigned(sizeof(std::uint32_t));
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

std::optional<std::uint64_t> ByteReader::read_u64() noexcept {
    return read_unsigned(sizeof(std::uint64_t));
}

std::optional<std::int8_t> ByteReader::read_i8() noexcept {
    const auto value = read_u8();
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::int8_t>(*value);
}

std::optional<std::int16_t> ByteReader::read_i16() noexcept {
    const auto value = read_u16();
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::int16_t>(*value);
}

std::optional<std::int32_t> ByteReader::read_i32() noexcept {
    const auto value = read_u32();
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(*value);
}

std::optional<std::int64_t> ByteReader::read_i64() noexcept {
    const auto value = read_u64();
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(*value);
}

std::optional<float> ByteReader::read_f32() noexcept {
    const auto value = read_u32();
    if (!value) {
        return std::nullopt;
    }
    return std::bit_cast<float>(*value);
}

std::optional<double> ByteReader::read_f64() noexcept {
    const auto value = read_u64();
    if (!value) {
        return std::nullopt;
    }
    return std::bit_cast<double>(*value);
}

} // namespace atlas::serialization
