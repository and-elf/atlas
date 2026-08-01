#include "atlas/core/random.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>

namespace atlas::core {
namespace {

TEST(Random, SameSeedProducesIdenticalU64Sequence) {
    Random first(42);
    Random second(42);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(first.next_u64(), second.next_u64());
    }
}

TEST(Random, SameSeedProducesIdenticalU32Sequence) {
    Random first(1234);
    Random second(1234);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(first.next_u32(), second.next_u32());
    }
}

TEST(Random, DifferentSeedsProduceDifferentSequences) {
    Random first(1);
    Random second(2);

    std::vector<std::uint64_t> first_values;
    std::vector<std::uint64_t> second_values;
    for (int i = 0; i < 10; ++i) {
        first_values.push_back(first.next_u64());
        second_values.push_back(second.next_u64());
    }

    EXPECT_NE(first_values, second_values);
}

TEST(Random, NextInRangeRespectsBounds) {
    Random random(7);

    for (int i = 0; i < 1000; ++i) {
        const auto value = random.next_in_range(10, 20);
        EXPECT_GE(value, 10U);
        EXPECT_LE(value, 20U);
    }
}

TEST(Random, NextInRangeAtDegenerateSingleValueRangeAlwaysReturnsIt) {
    Random random(99);

    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(random.next_in_range(5, 5), 5U);
    }
}

TEST(Random, NextInRangeIsReproducibleForSameSeed) {
    Random first(555);
    Random second(555);

    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(first.next_in_range(0, 1000), second.next_in_range(0, 1000));
    }
}

TEST(Random, NextInRangeRejectsInvertedBounds) {
    Random random(3);

    EXPECT_THROW([[maybe_unused]] const auto value = random.next_in_range(20, 10), std::invalid_argument);
}

TEST(Random, NextInRangeI64RespectsBoundsAcrossNegativeAndPositive) {
    Random random(7);

    for (int i = 0; i < 1000; ++i) {
        const auto value = random.next_in_range_i64(-10, 20);
        EXPECT_GE(value, -10);
        EXPECT_LE(value, 20);
    }
}

TEST(Random, NextInRangeI64HandlesFullNegativeRange) {
    Random random(11);

    for (int i = 0; i < 200; ++i) {
        const auto value = random.next_in_range_i64(-100, -50);
        EXPECT_GE(value, -100);
        EXPECT_LE(value, -50);
    }
}

TEST(Random, NextInRangeI64HandlesFullInt64Span) {
    Random random(13);

    for (int i = 0; i < 200; ++i) {
        const auto value = random.next_in_range_i64(std::numeric_limits<std::int64_t>::min(),
                                                    std::numeric_limits<std::int64_t>::max());
        EXPECT_GE(value, std::numeric_limits<std::int64_t>::min());
        EXPECT_LE(value, std::numeric_limits<std::int64_t>::max());
    }
}

TEST(Random, NextInRangeI64AtDegenerateSingleValueRangeAlwaysReturnsIt) {
    Random random(99);

    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(random.next_in_range_i64(-5, -5), -5);
    }
}

TEST(Random, NextInRangeI64IsReproducibleForSameSeed) {
    Random first(555);
    Random second(555);

    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(first.next_in_range_i64(-1000, 1000), second.next_in_range_i64(-1000, 1000));
    }
}

TEST(Random, NextInRangeI64RejectsInvertedBounds) {
    Random random(3);

    EXPECT_THROW([[maybe_unused]] const auto value = random.next_in_range_i64(10, -10),
                 std::invalid_argument);
}

TEST(Random, NextDoubleIsWithinUnitInterval) {
    Random random(21);

    for (int i = 0; i < 1000; ++i) {
        const auto value = random.next_double();
        EXPECT_GE(value, 0.0);
        EXPECT_LT(value, 1.0);
    }
}

TEST(Random, NextDoubleSameSeedProducesIdenticalSequence) {
    Random first(2024);
    Random second(2024);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(first.next_double(), second.next_double());
    }
}

TEST(Random, NextDoubleInRangeRespectsBounds) {
    Random random(31);

    for (int i = 0; i < 1000; ++i) {
        const auto value = random.next_double_in_range(-5.0, 5.0);
        EXPECT_GE(value, -5.0);
        EXPECT_LT(value, 5.0);
    }
}

TEST(Random, NextDoubleInRangeAtDegenerateSingleValueRangeAlwaysReturnsIt) {
    Random random(42);

    for (int i = 0; i < 20; ++i) {
        EXPECT_DOUBLE_EQ(random.next_double_in_range(2.5, 2.5), 2.5);
    }
}

TEST(Random, NextDoubleInRangeIsReproducibleForSameSeed) {
    Random first(777);
    Random second(777);

    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(first.next_double_in_range(-100.0, 100.0), second.next_double_in_range(-100.0, 100.0));
    }
}

TEST(Random, NextDoubleInRangeRejectsInvertedBounds) {
    Random random(3);

    EXPECT_THROW([[maybe_unused]] const auto value = random.next_double_in_range(10.0, -10.0),
                 std::invalid_argument);
}

} // namespace
} // namespace atlas::core
