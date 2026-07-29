#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace atlas::serialization {
namespace {

TEST(ByteWriter, WritesU8AsSingleByte) {
    ByteWriter writer;
    writer.write_u8(0x7A);

    ASSERT_EQ(writer.size(), 1U);
    EXPECT_EQ(writer.bytes()[0], std::byte{0x7A});
}

TEST(ByteWriter, WritesU16LittleEndian) {
    ByteWriter writer;
    writer.write_u16(0x1122);

    ASSERT_EQ(writer.size(), 2U);
    EXPECT_EQ(writer.bytes()[0], std::byte{0x22});
    EXPECT_EQ(writer.bytes()[1], std::byte{0x11});
}

TEST(ByteWriter, WritesU32LittleEndian) {
    ByteWriter writer;
    writer.write_u32(0x11223344);

    ASSERT_EQ(writer.size(), 4U);
    EXPECT_EQ(writer.bytes()[0], std::byte{0x44});
    EXPECT_EQ(writer.bytes()[1], std::byte{0x33});
    EXPECT_EQ(writer.bytes()[2], std::byte{0x22});
    EXPECT_EQ(writer.bytes()[3], std::byte{0x11});
}

TEST(ByteWriter, WritesU64LittleEndian) {
    ByteWriter writer;
    writer.write_u64(0x1122334455667788ULL);

    ASSERT_EQ(writer.size(), 8U);
    EXPECT_EQ(writer.bytes()[0], std::byte{0x88});
    EXPECT_EQ(writer.bytes()[1], std::byte{0x77});
    EXPECT_EQ(writer.bytes()[2], std::byte{0x66});
    EXPECT_EQ(writer.bytes()[3], std::byte{0x55});
    EXPECT_EQ(writer.bytes()[4], std::byte{0x44});
    EXPECT_EQ(writer.bytes()[5], std::byte{0x33});
    EXPECT_EQ(writer.bytes()[6], std::byte{0x22});
    EXPECT_EQ(writer.bytes()[7], std::byte{0x11});
}

TEST(ByteWriter, WritesSignedValuesAsTwosComplementBitPattern) {
    ByteWriter writer;
    writer.write_i8(-1);
    writer.write_i32(-1);

    ASSERT_EQ(writer.size(), 5U);
    for (const auto& byte : writer.bytes()) {
        EXPECT_EQ(byte, std::byte{0xFF});
    }
}

TEST(ByteWriter, MultipleWritesAppendSequentially) {
    ByteWriter writer;
    writer.write_u8(0x01);
    writer.write_u16(0x0203);

    ASSERT_EQ(writer.size(), 3U);
    EXPECT_EQ(writer.bytes()[0], std::byte{0x01});
    EXPECT_EQ(writer.bytes()[1], std::byte{0x03});
    EXPECT_EQ(writer.bytes()[2], std::byte{0x02});
}

TEST(ByteReader, ReadsLittleEndianByteOrder) {
    const std::vector<std::byte> data{std::byte{0x44}, std::byte{0x33}, std::byte{0x22}, std::byte{0x11}};
    ByteReader reader(data);

    const auto value = reader.read_u32();

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 0x11223344U);
}

TEST(ByteReader, RoundTripsEveryFixedWidthIntegerType) {
    ByteWriter writer;
    writer.write_u8(0xAB);
    writer.write_u16(0x1234);
    writer.write_u32(0x89ABCDEF);
    writer.write_u64(0x0123456789ABCDEFULL);
    writer.write_i8(-12);
    writer.write_i16(-1234);
    writer.write_i32(-123456);
    writer.write_i64(-1234567890123LL);

    ByteReader reader(writer.bytes());

    EXPECT_EQ(reader.read_u8(), 0xABU);
    EXPECT_EQ(reader.read_u16(), 0x1234U);
    EXPECT_EQ(reader.read_u32(), 0x89ABCDEFU);
    EXPECT_EQ(reader.read_u64(), 0x0123456789ABCDEFULL);
    EXPECT_EQ(reader.read_i8(), -12);
    EXPECT_EQ(reader.read_i16(), -1234);
    EXPECT_EQ(reader.read_i32(), -123456);
    EXPECT_EQ(reader.read_i64(), -1234567890123LL);
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(ByteReader, ReadingPastEndReturnsNulloptAndLeavesPositionUnchanged) {
    const std::vector<std::byte> data{std::byte{0x01}};
    ByteReader reader(data);

    const auto value = reader.read_u32();

    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(reader.position(), 0U);
    EXPECT_EQ(reader.remaining(), 1U);
}

TEST(ByteReader, ReadingFromEmptyBufferReturnsNullopt) {
    ByteReader reader({});

    EXPECT_FALSE(reader.read_u8().has_value());
}

TEST(ByteReader, EveryIntegerWidthFailsOnInsufficientBytes) {
    // Each read_* forwards failure from read_unsigned (unsigned widths) or
    // from the underlying read_u* (signed widths) — exercise every one of
    // those propagation paths, not just read_u8/read_u32 (covered above).
    {
        const std::vector<std::byte> empty;
        ByteReader reader(empty);
        EXPECT_FALSE(reader.read_i8().has_value());
    }
    {
        const std::vector<std::byte> one_byte{std::byte{0x00}};
        ByteReader reader(one_byte);
        EXPECT_FALSE(reader.read_u16().has_value());
        EXPECT_FALSE(reader.read_i16().has_value());
    }
    {
        const std::vector<std::byte> three_bytes(3, std::byte{0x00});
        ByteReader reader(three_bytes);
        EXPECT_FALSE(reader.read_u32().has_value());
        EXPECT_FALSE(reader.read_i32().has_value());
    }
    {
        const std::vector<std::byte> seven_bytes(7, std::byte{0x00});
        ByteReader reader(seven_bytes);
        EXPECT_FALSE(reader.read_u64().has_value());
        EXPECT_FALSE(reader.read_i64().has_value());
    }
}

TEST(ByteReader, PositionAndRemainingTrackCursorAdvancement) {
    const std::vector<std::byte> data{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};
    ByteReader reader(data);

    ASSERT_TRUE(reader.read_u16().has_value());

    EXPECT_EQ(reader.position(), 2U);
    EXPECT_EQ(reader.remaining(), 2U);
}

} // namespace
} // namespace atlas::serialization
