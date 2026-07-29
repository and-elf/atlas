#include "atlas/replication/resource_id_codec.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace atlas::replication {
namespace {

TEST(ResourceIdCodec, RoundTripsAnOrdinaryResourceId) {
    const auto id = ResourceId::from_name("characters/hero/mesh");

    serialization::ByteWriter writer;
    write_resource_id(writer, id);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_resource_id(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, id);
}

TEST(ResourceIdCodec, RoundTripsTheNullResourceId) {
    serialization::ByteWriter writer;
    write_resource_id(writer, ResourceId{});

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_resource_id(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_null());
}

TEST(ResourceIdCodec, WritesValueAsLittleEndianU64) {
    serialization::ByteWriter writer;
    write_resource_id(writer, ResourceId{.value = 0x0102030405060708ULL});

    ASSERT_EQ(writer.size(), 8U);
    const auto& bytes = writer.bytes();
    EXPECT_EQ(bytes[0], std::byte{0x08});
    EXPECT_EQ(bytes[1], std::byte{0x07});
    EXPECT_EQ(bytes[2], std::byte{0x06});
    EXPECT_EQ(bytes[3], std::byte{0x05});
    EXPECT_EQ(bytes[4], std::byte{0x04});
    EXPECT_EQ(bytes[5], std::byte{0x03});
    EXPECT_EQ(bytes[6], std::byte{0x02});
    EXPECT_EQ(bytes[7], std::byte{0x01});
}

TEST(ResourceIdCodec, MultipleResourceIdsSerializeSequentially) {
    const auto first_id = ResourceId::from_name("characters/hero/mesh");
    const auto second_id = ResourceId::from_name("characters/hero/texture");

    serialization::ByteWriter writer;
    write_resource_id(writer, first_id);
    write_resource_id(writer, second_id);

    serialization::ByteReader reader(writer.bytes());
    const auto first = read_resource_id(reader);
    const auto second = read_resource_id(reader);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, first_id);
    EXPECT_EQ(*second, second_id);
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(ResourceIdCodec, ReadFailsOnEmptyBuffer) {
    serialization::ByteReader reader({});

    EXPECT_FALSE(read_resource_id(reader).has_value());
}

TEST(ResourceIdCodec, ReadFailsOnTruncatedBuffer) {
    serialization::ByteWriter writer;
    writer.write_u32(0x11223344);

    serialization::ByteReader reader(writer.bytes());
    EXPECT_FALSE(read_resource_id(reader).has_value());
}

} // namespace
} // namespace atlas::replication
