#include "atlas/entity/entity_ref.hpp"
#include "atlas/replication/property_codec.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

namespace atlas::replication {
namespace {

// Deliberately not health::Health (a demo capability type, out of reach from
// this library's tests) - a small plain aggregate exercising the same shape
// (two int32 fields) plus a second fixture covering every other primitive
// width this codec supports, since Health alone would only prove int32.
struct TwoInt32Fields {
    std::int32_t current = 0;
    std::int32_t maximum = 0;

    friend constexpr bool operator==(const TwoInt32Fields&, const TwoInt32Fields&) noexcept = default;
};

struct EveryPrimitiveWidth {
    std::int8_t a = 0;
    std::uint8_t b = 0;
    std::int16_t c = 0;
    std::uint16_t d = 0;
    std::int32_t e = 0;
    std::uint32_t f = 0;
    std::int64_t g = 0;
    std::uint64_t h = 0;
    float i = 0;
    double j = 0;

    friend constexpr bool operator==(const EveryPrimitiveWidth&,
                                     const EveryPrimitiveWidth&) noexcept = default;
};

struct SingleFloatField {
    float value = 0;

    friend constexpr bool operator==(const SingleFloatField&, const SingleFloatField&) noexcept = default;
};

// Struct-typed fields (issue #21) - EntityRef/ResourceId are themselves
// plain FieldVisitable aggregates of primitives, so a property containing
// one should recurse rather than fail to compile.
struct WithEntityRefField {
    EntityRef target;
    std::int32_t amount = 0;

    friend constexpr bool operator==(const WithEntityRefField&, const WithEntityRefField&) noexcept = default;
};

struct WithResourceIdField {
    ResourceId icon;
    std::int32_t count = 0;

    friend constexpr bool operator==(const WithResourceIdField&,
                                     const WithResourceIdField&) noexcept = default;
};

// Two levels deep - Outer contains Inner (itself a struct), proving this is
// genuine recursion, not a single extra special case for exactly one level
// of nesting.
struct Inner {
    std::int32_t x = 0;
    std::int32_t y = 0;

    friend constexpr bool operator==(const Inner&, const Inner&) noexcept = default;
};

struct OuterWithNestedStruct {
    Inner position;
    std::int32_t scale = 0;

    friend constexpr bool operator==(const OuterWithNestedStruct&,
                                     const OuterWithNestedStruct&) noexcept = default;
};

struct DoublyNested {
    OuterWithNestedStruct outer;
    std::int32_t tag = 0;

    friend constexpr bool operator==(const DoublyNested&, const DoublyNested&) noexcept = default;
};

TEST(PropertyCodec, RoundTripsATwoFieldAggregate) {
    const TwoInt32Fields original{.current = 7, .maximum = 10};

    serialization::ByteWriter writer;
    write_property_fields(writer, original);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_fields<TwoInt32Fields>(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(PropertyCodec, RoundTripsEveryPrimitiveFieldWidth) {
    const EveryPrimitiveWidth original{
        .a = -1, .b = 2, .c = -3, .d = 4, .e = -5, .f = 6, .g = -7, .h = 8, .i = 9.5F, .j = 10.5};

    serialization::ByteWriter writer;
    write_property_fields(writer, original);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_fields<EveryPrimitiveWidth>(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(PropertyCodec, RoundTripsASingleFieldAggregate) {
    const SingleFloatField original{.value = 3.5F};

    serialization::ByteWriter writer;
    write_property_fields(writer, original);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_fields<SingleFloatField>(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(PropertyCodec, WritesExactlyTheSumOfEachFieldsWidth) {
    serialization::ByteWriter writer;
    write_property_fields(writer, TwoInt32Fields{.current = 1, .maximum = 2});

    EXPECT_EQ(writer.size(), 8U); // two int32 fields, 4 bytes each
}

TEST(PropertyCodec, ReadFailsOnEmptyBuffer) {
    serialization::ByteReader reader({});

    EXPECT_FALSE(read_property_fields<TwoInt32Fields>(reader).has_value());
}

TEST(PropertyCodec, ReadFailsOnABufferTruncatedMidField) {
    serialization::ByteWriter writer;
    writer.write_i32(7); // only the first of two fields

    serialization::ByteReader reader(writer.bytes());
    EXPECT_FALSE(read_property_fields<TwoInt32Fields>(reader).has_value());
}

TEST(PropertyCodec, MultiplePropertiesSerializeSequentially) {
    serialization::ByteWriter writer;
    write_property_fields(writer, TwoInt32Fields{.current = 1, .maximum = 2});
    write_property_fields(writer, TwoInt32Fields{.current = 3, .maximum = 4});

    serialization::ByteReader reader(writer.bytes());
    const auto first = read_property_fields<TwoInt32Fields>(reader);
    const auto second = read_property_fields<TwoInt32Fields>(reader);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, (TwoInt32Fields{.current = 1, .maximum = 2}));
    EXPECT_EQ(*second, (TwoInt32Fields{.current = 3, .maximum = 4}));
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(PropertyCodec, RoundTripsANestedEntityRefField) {
    const WithEntityRefField original{.target = EntityRef{.index = 7, .generation = 2}, .amount = 42};

    serialization::ByteWriter writer;
    write_property_fields(writer, original);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_fields<WithEntityRefField>(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(PropertyCodec, WritesANestedEntityRefFieldAsTwoU32sMatchingEntityRefCodecsOwnShape) {
    // entity_ref_codec.hpp's write_entity_ref puts index then generation on
    // the wire as two plain little-endian u32s - the generic recursive
    // codec should produce the exact same bytes for the nested field,
    // purely as a consequence of visiting EntityRef's own two uint32_t
    // fields in declaration order, not because the two codecs share code.
    serialization::ByteWriter writer;
    write_property_fields(writer,
                          WithEntityRefField{.target = EntityRef{.index = 1, .generation = 0}, .amount = 0});

    ASSERT_EQ(writer.size(), 12U); // index(u32) + generation(u32) + amount(i32)
    const auto& bytes = writer.bytes();
    EXPECT_EQ(bytes[0], std::byte{0x01}); // index = 1, little-endian
    EXPECT_EQ(bytes[4], std::byte{0x00}); // generation = 0
}

TEST(PropertyCodec, RoundTripsANestedResourceIdField) {
    const WithResourceIdField original{.icon = ResourceId::from_name("icons/fireball"), .count = 3};

    serialization::ByteWriter writer;
    write_property_fields(writer, original);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_fields<WithResourceIdField>(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(PropertyCodec, RoundTripsATwoLevelsDeepNestedStruct) {
    const DoublyNested original{.outer = OuterWithNestedStruct{.position = Inner{.x = 1, .y = 2}, .scale = 3},
                                .tag = 4};

    serialization::ByteWriter writer;
    write_property_fields(writer, original);

    serialization::ByteReader reader(writer.bytes());
    const auto result = read_property_fields<DoublyNested>(reader);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(PropertyCodec, ReadFailsWhenANestedStructFieldIsTruncated) {
    serialization::ByteWriter writer;
    writer.write_u32(7); // only EntityRef::index, not generation, and no amount at all

    serialization::ByteReader reader(writer.bytes());
    EXPECT_FALSE(read_property_fields<WithEntityRefField>(reader).has_value());
}

} // namespace
} // namespace atlas::replication
