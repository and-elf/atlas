#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace atlas::serialization {

// Reverses ByteWriter's explicit little-endian encoding. An encapsulated
// class because it protects a real invariant across its own operations: the
// read cursor never advances past the end of the backing buffer, and a read
// that would run past it fails explicitly (std::nullopt) rather than reading
// past-the-end / uninitialized memory (spec §4 forbids exactly that).
class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) noexcept : data_(data) {}

    [[nodiscard]] std::optional<std::uint8_t> read_u8() noexcept;
    [[nodiscard]] std::optional<std::uint16_t> read_u16() noexcept;
    [[nodiscard]] std::optional<std::uint32_t> read_u32() noexcept;
    [[nodiscard]] std::optional<std::uint64_t> read_u64() noexcept;

    [[nodiscard]] std::optional<std::int8_t> read_i8() noexcept;
    [[nodiscard]] std::optional<std::int16_t> read_i16() noexcept;
    [[nodiscard]] std::optional<std::int32_t> read_i32() noexcept;
    [[nodiscard]] std::optional<std::int64_t> read_i64() noexcept;

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - position_; }

private:
    [[nodiscard]] std::optional<std::uint64_t> read_unsigned(std::size_t width) noexcept;

    std::span<const std::byte> data_;
    std::size_t position_ = 0;
};

} // namespace atlas::serialization
