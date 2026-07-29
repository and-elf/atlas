#include "atlas/core/time.hpp"

#include <gtest/gtest.h>

namespace atlas::core {
namespace {

TEST(Time, DefaultConstructsToTickZero) {
    constexpr Time time{};

    EXPECT_EQ(time.ticks, 0U);
}

TEST(Time, AdvancesByTickCount) {
    constexpr Time start{10};
    constexpr Time advanced = start + 5U;

    EXPECT_EQ(advanced.ticks, 15U);
}

TEST(Time, PlusEqualsAdvancesInPlace) {
    Time time{10};
    time += 5U;

    EXPECT_EQ(time.ticks, 15U);
}

TEST(Time, DifferenceIsSignedTickDelta) {
    constexpr Time earlier{10};
    constexpr Time later{25};

    EXPECT_EQ(later - earlier, 15);
    EXPECT_EQ(earlier - later, -15);
}

TEST(Time, OrdersByTickCount) {
    constexpr Time earlier{10};
    constexpr Time later{25};

    EXPECT_LT(earlier, later);
    EXPECT_EQ(earlier, (Time{10}));
    EXPECT_NE(earlier, later);
}

TEST(Time, ConvertsToDurationUsingTicksPerSecond) {
    constexpr Time one_second{Time::ticks_per_second};

    const auto duration = one_second.to_duration();

    EXPECT_DOUBLE_EQ(duration.count(), 1.0);
}

TEST(Time, ConvertsPartialTickCountToFractionalSeconds) {
    constexpr Time half_second{Time::ticks_per_second / 2};

    const auto duration = half_second.to_duration();

    EXPECT_DOUBLE_EQ(duration.count(), 0.5);
}

} // namespace
} // namespace atlas::core
