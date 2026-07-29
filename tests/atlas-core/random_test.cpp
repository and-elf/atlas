#include "atlas/core/random.hpp"

#include <cstdint>
#include <gtest/gtest.h>
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

} // namespace
} // namespace atlas::core
