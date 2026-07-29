#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace atlas::serialization {

// Explicit fixed-endianness (little-endian) byte encoder for fixed-width
// integers, built to avoid the platform-dependent implicit encoding that
// native struct layout / memcpy would introduce (spec §4: bit-exact
// determinism across machines and full-session replay demands an encoding
// that never depends on host endianness, even though every platform Atlas
// currently targets happens to be little-endian already).
//
// An encapsulated class rather than a bare buffer: the invariant it protects
// is that bytes only ever enter the buffer through an explicit-width,
// explicit-endianness write_* call, never via a raw append that could leak
// native layout.
class ByteWriter {
public:
    void write_u8(std::uint8_t value);
    void write_u16(std::uint16_t value);
    void write_u32(std::uint32_t value);
    void write_u64(std::uint64_t value);

    // Signed values are written as the two's complement bit pattern of their
    // unsigned counterpart width — decoding recovers the original value via
    // the equally well-defined (C++20 [conv.integral]) reverse conversion.
    void write_i8(std::int8_t value);
    void write_i16(std::int16_t value);
    void write_i32(std::int32_t value);
    void write_i64(std::int64_t value);

    // Bits are reinterpreted via std::bit_cast into the same-width unsigned
    // integer and handed to write_u32/write_u64 above — reusing that
    // little-endian encoding rather than duplicating it — so NaN payloads,
    // signed zero, and infinities all round-trip exactly as written, which
    // spec §4's bit-exact determinism guarantee requires.
    void write_f32(float value);
    void write_f64(double value);

    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return buffer_; }
    [[nodiscard]] std::size_t size() const noexcept { return buffer_.size(); }

private:
    // Templated on the operand's own width rather than taking a runtime
    // (value, width) pair — two same-type runtime parameters would be an
    // easily-swapped-by-mistake hazard for no benefit, since the width is
    // always known at the call site as sizeof(Unsigned).
    template <typename Unsigned> void write_fixed(Unsigned value);

    std::vector<std::byte> buffer_;
};

} // namespace atlas::serialization
