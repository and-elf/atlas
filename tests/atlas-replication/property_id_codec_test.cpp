#include "atlas/replication/property_id.hpp"
#include "atlas/replication/property_id_codec.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace atlas::replication {
namespace {

TEST(PropertyIdCodec, RoundTripsAnOrdinaryPropertyId) {
    const auto id = PropertyId::from_name("Health");

    serialization::ByteWriter writer;
    write_property_id(writer, id);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_id(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, id);
}

TEST(PropertyIdCodec, RoundTripsTheNullPropertyId) {
    serialization::ByteWriter writer;
    write_property_id(writer, PropertyId{});

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_id(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_null());
}

TEST(PropertyIdCodec, WritesValueAsLittleEndianU64) {
    serialization::ByteWriter writer;
    write_property_id(writer, PropertyId{.value = 0x0102030405060708ULL});

    ASSERT_EQ(writer.size(), 8U);
    const auto& bytes = writer.bytes();
    EXPECT_EQ(bytes[0], std::byte{0x08});
    EXPECT_EQ(bytes[7], std::byte{0x01});
}

TEST(PropertyIdCodec, ReadFailsOnEmptyBuffer) {
    serialization::ByteReader reader({});

    EXPECT_FALSE(read_property_id(reader).has_value());
}

TEST(PropertyIdCodec, ReadFailsOnTruncatedBuffer) {
    serialization::ByteWriter writer;
    writer.write_u32(0x11223344);

    serialization::ByteReader reader(writer.bytes());
    EXPECT_FALSE(read_property_id(reader).has_value());
}

} // namespace
} // namespace atlas::replication
