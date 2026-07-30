#include "atlas/replication/property_codec.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

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

} // namespace
} // namespace atlas::replication
