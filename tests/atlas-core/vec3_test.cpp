#include "atlas/core/vec3.hpp"

#include <gtest/gtest.h>

namespace atlas::core {
namespace {

TEST(Vec3, DefaultConstructsToZero) {
    constexpr Vec3 vec{};

    EXPECT_FLOAT_EQ(vec.x, 0.0F);
    EXPECT_FLOAT_EQ(vec.y, 0.0F);
    EXPECT_FLOAT_EQ(vec.z, 0.0F);
}

TEST(Vec3, AggregateInitializationSetsEachComponentIndependently) {
    constexpr Vec3 vec{.x = 1.0F, .y = -2.0F, .z = 3.5F};

    EXPECT_FLOAT_EQ(vec.x, 1.0F);
    EXPECT_FLOAT_EQ(vec.y, -2.0F);
    EXPECT_FLOAT_EQ(vec.z, 3.5F);
}

} // namespace
} // namespace atlas::core
