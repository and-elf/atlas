#include "atlas/render/transform.hpp"

#include <cmath>
#include <gtest/gtest.h>

namespace atlas::render {
namespace {

TEST(Lerp, FloatAtZeroReturnsFirstValue) {
    EXPECT_FLOAT_EQ(lerp(1.0F, 5.0F, 0.0), 1.0F);
}

TEST(Lerp, FloatAtOneReturnsSecondValue) {
    EXPECT_FLOAT_EQ(lerp(1.0F, 5.0F, 1.0), 5.0F);
}

TEST(Lerp, FloatAtHalfReturnsMidpoint) {
    EXPECT_FLOAT_EQ(lerp(0.0F, 10.0F, 0.5), 5.0F);
}

TEST(Lerp, Vec3InterpolatesEachComponentIndependently) {
    const Vec3 start{.x = 0.0F, .y = 0.0F, .z = 0.0F};
    const Vec3 end{.x = 10.0F, .y = -10.0F, .z = 4.0F};

    const Vec3 result = lerp(start, end, 0.5);

    EXPECT_FLOAT_EQ(result.x, 5.0F);
    EXPECT_FLOAT_EQ(result.y, -5.0F);
    EXPECT_FLOAT_EQ(result.z, 2.0F);
}

TEST(Nlerp, AtZeroReturnsFirstRotationNormalized) {
    const Quaternion start{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F};
    const Quaternion end{.x = 1.0F, .y = 0.0F, .z = 0.0F, .w = 0.0F};

    const Quaternion result = nlerp(start, end, 0.0);

    EXPECT_NEAR(result.x, 0.0F, 1e-6);
    EXPECT_NEAR(result.w, 1.0F, 1e-6);
}

TEST(Nlerp, AtOneReturnsSecondRotationNormalized) {
    const Quaternion start{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F};
    const Quaternion end{.x = 1.0F, .y = 0.0F, .z = 0.0F, .w = 0.0F};

    const Quaternion result = nlerp(start, end, 1.0);

    EXPECT_NEAR(result.x, 1.0F, 1e-6);
    EXPECT_NEAR(result.w, 0.0F, 1e-6);
}

TEST(Nlerp, MidpointIsNormalized) {
    const Quaternion start{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F};
    const Quaternion end{.x = 1.0F, .y = 0.0F, .z = 0.0F, .w = 0.0F};

    const Quaternion result = nlerp(start, end, 0.5);

    const double length = std::sqrt(static_cast<double>(result.x) * static_cast<double>(result.x) +
                                    static_cast<double>(result.y) * static_cast<double>(result.y) +
                                    static_cast<double>(result.z) * static_cast<double>(result.z) +
                                    static_cast<double>(result.w) * static_cast<double>(result.w));

    EXPECT_NEAR(length, 1.0, 1e-6);
}

TEST(Nlerp, OppositeQuaternionsFallBackToTheSecondRotationRatherThanDividingByZero) {
    // a and b represent the same rotation under quaternion double-cover
    // (b == -a component-wise), so their linear sum is exactly zero -
    // nlerp must not divide by that, and falls back to returning b.
    const Quaternion start{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F};
    const Quaternion end{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = -1.0F};

    const Quaternion result = nlerp(start, end, 0.5);

    EXPECT_FLOAT_EQ(result.w, -1.0F);
}

TEST(TransformLerp, InterpolatesPositionRotationAndScaleTogether) {
    const Transform start{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .scale = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
    };
    const Transform end{
        .position = {.x = 10.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .scale = {.x = 3.0F, .y = 1.0F, .z = 1.0F},
    };

    const Transform result = lerp(start, end, 0.5);

    EXPECT_FLOAT_EQ(result.position.x, 5.0F);
    EXPECT_FLOAT_EQ(result.scale.x, 2.0F);
}

TEST(Transform, DefaultScaleIsUnit) {
    const Transform transform;

    EXPECT_FLOAT_EQ(transform.scale.x, 1.0F);
    EXPECT_FLOAT_EQ(transform.scale.y, 1.0F);
    EXPECT_FLOAT_EQ(transform.scale.z, 1.0F);
}

} // namespace
} // namespace atlas::render
