// Direct unit tests for health's own logic (its wire codec), as opposed to
// combat_scenario_test.cpp's end-to-end scenario coverage - mirroring how
// every atlas-* library gets its own tests/<library>/ directory rather than
// only ever being exercised indirectly through a larger scenario.
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <span>

#include "health/health.hpp"

namespace atlas::health {
namespace {

TEST(WriteReadHealth, RoundTripsAnOrdinaryValue) {
    serialization::ByteWriter writer;
    write_health(writer, Health{.current = 7, .maximum = 10});

    serialization::ByteReader reader(writer.bytes());
    const auto decoded = read_health(reader);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->current, 7);
    EXPECT_EQ(decoded->maximum, 10);
}

TEST(WriteReadHealth, FailsExplicitlyWhenTheCurrentFieldIsTruncated) {
    serialization::ByteReader reader(std::span<const std::byte>{});

    EXPECT_FALSE(read_health(reader).has_value());
}

TEST(WriteReadHealth, FailsExplicitlyWhenTheMaximumFieldIsTruncated) {
    serialization::ByteWriter writer;
    writer.write_i32(7); // only the current field - maximum is missing

    serialization::ByteReader reader(writer.bytes());

    EXPECT_FALSE(read_health(reader).has_value());
}

} // namespace
} // namespace atlas::health
