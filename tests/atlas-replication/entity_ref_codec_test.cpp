#include "atlas/entity/entity_ref.hpp"
#include "atlas/replication/entity_ref_codec.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace atlas::replication {
namespace {

TEST(EntityRefCodec, RoundTripsAnOrdinaryEntityRef) {
    serialization::ByteWriter writer;
    write_entity_ref(writer, EntityRef{.index = 7, .generation = 3});

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_entity_ref(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (EntityRef{.index = 7, .generation = 3}));
}

TEST(EntityRefCodec, RoundTripsTheNullEntityRef) {
    serialization::ByteWriter writer;
    write_entity_ref(writer, EntityRef{});

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_entity_ref(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_null());
}

TEST(EntityRefCodec, WritesIndexThenGenerationLittleEndian) {
    serialization::ByteWriter writer;
    write_entity_ref(writer, EntityRef{.index = 0x11223344, .generation = 0x01020304});

    ASSERT_EQ(writer.size(), 8U);
    const auto& bytes = writer.bytes();
    // index first, little-endian
    EXPECT_EQ(bytes[0], std::byte{0x44});
    EXPECT_EQ(bytes[1], std::byte{0x33});
    EXPECT_EQ(bytes[2], std::byte{0x22});
    EXPECT_EQ(bytes[3], std::byte{0x11});
    // then generation, little-endian
    EXPECT_EQ(bytes[4], std::byte{0x04});
    EXPECT_EQ(bytes[5], std::byte{0x03});
    EXPECT_EQ(bytes[6], std::byte{0x02});
    EXPECT_EQ(bytes[7], std::byte{0x01});
}

TEST(EntityRefCodec, MultipleEntityRefsSerializeSequentially) {
    serialization::ByteWriter writer;
    write_entity_ref(writer, EntityRef{.index = 1, .generation = 0});
    write_entity_ref(writer, EntityRef{.index = 2, .generation = 5});

    serialization::ByteReader reader(writer.bytes());
    const auto first = read_entity_ref(reader);
    const auto second = read_entity_ref(reader);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, (EntityRef{.index = 1, .generation = 0}));
    EXPECT_EQ(*second, (EntityRef{.index = 2, .generation = 5}));
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(EntityRefCodec, ReadFailsOnEmptyBuffer) {
    serialization::ByteReader reader({});

    EXPECT_FALSE(read_entity_ref(reader).has_value());
}

TEST(EntityRefCodec, ReadFailsWhenOnlyIndexIsPresent) {
    serialization::ByteWriter writer;
    writer.write_u32(0x11223344);

    serialization::ByteReader reader(writer.bytes());
    EXPECT_FALSE(read_entity_ref(reader).has_value());
}

TEST(EntityRefCodec, ReadFailsWhenGenerationIsTruncated) {
    serialization::ByteWriter writer;
    writer.write_u32(1);
    writer.write_u8(0xAB);

    serialization::ByteReader reader(writer.bytes());
    EXPECT_FALSE(read_entity_ref(reader).has_value());
}

} // namespace
} // namespace atlas::replication
