#include "atlas/core/quaternion.hpp"

#include <gtest/gtest.h>

namespace atlas::core {
namespace {

TEST(Quaternion, DefaultConstructsToIdentityRotation) {
    constexpr Quaternion quat{};

    EXPECT_FLOAT_EQ(quat.x, 0.0F);
    EXPECT_FLOAT_EQ(quat.y, 0.0F);
    EXPECT_FLOAT_EQ(quat.z, 0.0F);
    EXPECT_FLOAT_EQ(quat.w, 1.0F);
}

TEST(Quaternion, AggregateInitializationSetsEachComponentIndependently) {
    constexpr Quaternion quat{.x = 0.1F, .y = 0.2F, .z = 0.3F, .w = 0.9F};

    EXPECT_FLOAT_EQ(quat.x, 0.1F);
    EXPECT_FLOAT_EQ(quat.y, 0.2F);
    EXPECT_FLOAT_EQ(quat.z, 0.3F);
    EXPECT_FLOAT_EQ(quat.w, 0.9F);
}

} // namespace
} // namespace atlas::core
