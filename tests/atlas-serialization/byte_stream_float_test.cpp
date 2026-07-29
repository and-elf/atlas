#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <bit>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>

namespace atlas::serialization {
namespace {

TEST(ByteWriter, WritesF32LittleEndian) {
    ByteWriter writer;
    writer.write_f32(1.0F);

    ASSERT_EQ(writer.size(), 4U);
    // 1.0F is 0x3F800000 in IEEE-754 binary32, little-endian byte order.
    EXPECT_EQ(writer.bytes()[0], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[1], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[2], std::byte{0x80});
    EXPECT_EQ(writer.bytes()[3], std::byte{0x3F});
}

TEST(ByteWriter, WritesF64LittleEndian) {
    ByteWriter writer;
    writer.write_f64(1.0);

    ASSERT_EQ(writer.size(), 8U);
    // 1.0 is 0x3FF0000000000000 in IEEE-754 binary64, little-endian byte order.
    EXPECT_EQ(writer.bytes()[0], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[1], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[2], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[3], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[4], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[5], std::byte{0x00});
    EXPECT_EQ(writer.bytes()[6], std::byte{0xF0});
    EXPECT_EQ(writer.bytes()[7], std::byte{0x3F});
}

TEST(ByteReader, RoundTripsOrdinaryF32AndF64Values) {
    ByteWriter writer;
    writer.write_f32(3.5F);
    writer.write_f64(-std::numbers::e);

    ByteReader reader(writer.bytes());

    const auto f32_value = reader.read_f32();
    ASSERT_TRUE(f32_value.has_value());
    EXPECT_EQ(*f32_value, 3.5F);

    const auto f64_value = reader.read_f64();
    ASSERT_TRUE(f64_value.has_value());
    EXPECT_EQ(*f64_value, -std::numbers::e);
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(ByteReader, PositiveAndNegativeZeroF32SurviveAsDistinctBitPatterns) {
    ByteWriter writer;
    writer.write_f32(0.0F);
    writer.write_f32(-0.0F);

    ByteReader reader(writer.bytes());

    const auto positive_zero = reader.read_f32();
    const auto negative_zero = reader.read_f32();
    ASSERT_TRUE(positive_zero.has_value());
    ASSERT_TRUE(negative_zero.has_value());

    // == would treat +0.0F and -0.0F as equal; compare the raw bit patterns
    // instead so a silent sign-collapsing bug would actually be caught.
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*positive_zero), 0x00000000U);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*negative_zero), 0x80000000U);
}

TEST(ByteReader, PositiveAndNegativeZeroF64SurviveAsDistinctBitPatterns) {
    ByteWriter writer;
    writer.write_f64(0.0);
    writer.write_f64(-0.0);

    ByteReader reader(writer.bytes());

    const auto positive_zero = reader.read_f64();
    const auto negative_zero = reader.read_f64();
    ASSERT_TRUE(positive_zero.has_value());
    ASSERT_TRUE(negative_zero.has_value());

    EXPECT_EQ(std::bit_cast<std::uint64_t>(*positive_zero), 0x0000000000000000ULL);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(*negative_zero), 0x8000000000000000ULL);
}

TEST(ByteReader, NanBitPatternSurvivesF32RoundTrip) {
    // NaN != NaN under ==, and a NaN's mantissa payload is not guaranteed to
    // be preserved by every operation that touches it — so the only faithful
    // check is comparing raw bits, not std::isnan.
    const auto nan_bits = std::uint32_t{0x7FC0BEEF};
    const auto nan_value = std::bit_cast<float>(nan_bits);

    ByteWriter writer;
    writer.write_f32(nan_value);

    ByteReader reader(writer.bytes());
    const auto read_value = reader.read_f32();

    ASSERT_TRUE(read_value.has_value());
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*read_value), nan_bits);
}

TEST(ByteReader, NanBitPatternSurvivesF64RoundTrip) {
    const auto nan_bits = std::uint64_t{0x7FF80000CAFEBEEFULL};
    const auto nan_value = std::bit_cast<double>(nan_bits);

    ByteWriter writer;
    writer.write_f64(nan_value);

    ByteReader reader(writer.bytes());
    const auto read_value = reader.read_f64();

    ASSERT_TRUE(read_value.has_value());
    EXPECT_EQ(std::bit_cast<std::uint64_t>(*read_value), nan_bits);
}

TEST(ByteReader, InfinitiesRoundTripF32) {
    ByteWriter writer;
    writer.write_f32(std::numeric_limits<float>::infinity());
    writer.write_f32(-std::numeric_limits<float>::infinity());

    ByteReader reader(writer.bytes());

    const auto positive_infinity = reader.read_f32();
    const auto negative_infinity = reader.read_f32();
    ASSERT_TRUE(positive_infinity.has_value());
    ASSERT_TRUE(negative_infinity.has_value());
    EXPECT_EQ(*positive_infinity, std::numeric_limits<float>::infinity());
    EXPECT_EQ(*negative_infinity, -std::numeric_limits<float>::infinity());
}

TEST(ByteReader, InfinitiesRoundTripF64) {
    ByteWriter writer;
    writer.write_f64(std::numeric_limits<double>::infinity());
    writer.write_f64(-std::numeric_limits<double>::infinity());

    ByteReader reader(writer.bytes());

    const auto positive_infinity = reader.read_f64();
    const auto negative_infinity = reader.read_f64();
    ASSERT_TRUE(positive_infinity.has_value());
    ASSERT_TRUE(negative_infinity.has_value());
    EXPECT_EQ(*positive_infinity, std::numeric_limits<double>::infinity());
    EXPECT_EQ(*negative_infinity, -std::numeric_limits<double>::infinity());
}

TEST(ByteReader, ReadF32FailsExplicitlyOnInsufficientBytes) {
    const std::vector<std::byte> three_bytes(3, std::byte{0x00});
    ByteReader reader(three_bytes);

    const auto value = reader.read_f32();

    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(reader.position(), 0U);
    EXPECT_EQ(reader.remaining(), 3U);
}

TEST(ByteReader, ReadF64FailsExplicitlyOnInsufficientBytes) {
    const std::vector<std::byte> seven_bytes(7, std::byte{0x00});
    ByteReader reader(seven_bytes);

    const auto value = reader.read_f64();

    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(reader.position(), 0U);
    EXPECT_EQ(reader.remaining(), 7U);
}

} // namespace
} // namespace atlas::serialization
