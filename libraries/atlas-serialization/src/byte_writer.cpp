#include "atlas/serialization/byte_writer.hpp"

#include <bit>
#include <limits>

namespace atlas::serialization {

// std::bit_cast between float/double and a same-width unsigned integer only
// preserves bits the way write_f32/write_f64 rely on if the platform's
// float/double actually is IEEE-754 binary32/binary64 — true for every
// target in CLAUDE.md's Build & Toolchain list today, but not guaranteed by
// the C++ standard for an arbitrary implementation, so the assumption is
// pinned here rather than left implicit.
static_assert(std::numeric_limits<float>::is_iec559, "atlas-serialization assumes IEEE-754 binary32 float");
static_assert(std::numeric_limits<double>::is_iec559, "atlas-serialization assumes IEEE-754 binary64 double");

// Defined here rather than in the header: write_fixed is private and only
// ever instantiated from the write_* calls below, all in this translation
// unit, so no other TU needs to see the definition.
template <typename Unsigned> void ByteWriter::write_fixed(Unsigned value) {
    for (std::size_t i = 0; i < sizeof(Unsigned); ++i) {
        buffer_.push_back(static_cast<std::byte>((value >> (i * 8U)) & Unsigned{0xFFU}));
    }
}

void ByteWriter::write_u8(std::uint8_t value) {
    write_fixed(value);
}
void ByteWriter::write_u16(std::uint16_t value) {
    write_fixed(value);
}
void ByteWriter::write_u32(std::uint32_t value) {
    write_fixed(value);
}
void ByteWriter::write_u64(std::uint64_t value) {
    write_fixed(value);
}

void ByteWriter::write_i8(std::int8_t value) {
    write_fixed(static_cast<std::uint8_t>(value));
}
void ByteWriter::write_i16(std::int16_t value) {
    write_fixed(static_cast<std::uint16_t>(value));
}
void ByteWriter::write_i32(std::int32_t value) {
    write_fixed(static_cast<std::uint32_t>(value));
}
void ByteWriter::write_i64(std::int64_t value) {
    write_fixed(static_cast<std::uint64_t>(value));
}

void ByteWriter::write_f32(float value) {
    write_u32(std::bit_cast<std::uint32_t>(value));
}
void ByteWriter::write_f64(double value) {
    write_u64(std::bit_cast<std::uint64_t>(value));
}

} // namespace atlas::serialization
