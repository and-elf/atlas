#include "atlas/render/transform.hpp"

#include <array>
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

// to_model_matrix (issue #154): builds a row-major 4x4 model matrix from a
// Transform - the model-matrix half of "apply each DrawCommand's Transform as
// a model matrix... against a fixed identity/placeholder view-projection"
// (no Camera exists yet, see this library's README). Row-major, translation
// in the last column, so element [row * 4 + col] is at index row*4+col;
// verified against the corresponding SDL_shadercross-reflected HLSL cbuffer
// layout (row_major float4x4) via a standalone compile+reflect smoke check
// before this shader/matrix pairing was committed.
TEST(ToModelMatrix, IdentityTransformProducesTheIdentityMatrix) {
    const Transform transform{};

    const std::array<float, 16> matrix = to_model_matrix(transform);

    const std::array<float, 16> expected{
        1.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        1.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        1.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        1.0F,
    };
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FLOAT_EQ(matrix.at(i), expected.at(i)) << "index " << i;
    }
}

TEST(ToModelMatrix, TranslationOnlyPlacesPositionInTheLastColumn) {
    const Transform transform{
        .position = {.x = 2.0F, .y = 3.0F, .z = 4.0F},
        .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .scale = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
    };

    const std::array<float, 16> matrix = to_model_matrix(transform);

    EXPECT_FLOAT_EQ(matrix[3], 2.0F);
    EXPECT_FLOAT_EQ(matrix[7], 3.0F);
    EXPECT_FLOAT_EQ(matrix[11], 4.0F);
    // The rotation/scale 3x3 block is untouched by translation alone.
    EXPECT_FLOAT_EQ(matrix[0], 1.0F);
    EXPECT_FLOAT_EQ(matrix[5], 1.0F);
    EXPECT_FLOAT_EQ(matrix[10], 1.0F);
    // Bottom row is always [0, 0, 0, 1] for an affine transform.
    EXPECT_FLOAT_EQ(matrix[12], 0.0F);
    EXPECT_FLOAT_EQ(matrix[13], 0.0F);
    EXPECT_FLOAT_EQ(matrix[14], 0.0F);
    EXPECT_FLOAT_EQ(matrix[15], 1.0F);
}

TEST(ToModelMatrix, ScaleOnlyScalesTheDiagonalWithoutTranslating) {
    const Transform transform{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .scale = {.x = 2.0F, .y = 3.0F, .z = 4.0F},
    };

    const std::array<float, 16> matrix = to_model_matrix(transform);

    EXPECT_FLOAT_EQ(matrix[0], 2.0F);
    EXPECT_FLOAT_EQ(matrix[5], 3.0F);
    EXPECT_FLOAT_EQ(matrix[10], 4.0F);
    EXPECT_FLOAT_EQ(matrix[3], 0.0F);
    EXPECT_FLOAT_EQ(matrix[7], 0.0F);
    EXPECT_FLOAT_EQ(matrix[11], 0.0F);
}

TEST(ToModelMatrix, NinetyDegreeRotationAboutZProducesTheExpectedRotationBlock) {
    // Quaternion (0, 0, sin(45deg), cos(45deg)) - a 90 degree rotation about
    // Z - maps the X axis onto the Y axis: row0=[0,-1,0], row1=[1,0,0],
    // row2=[0,0,1] (independently derived from the standard quaternion to
    // rotation-matrix formula, not copied from the implementation under
    // test).
    constexpr float half_angle = 0.78539816339744830962F; // 45 degrees in radians
    const Transform transform{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {.x = 0.0F, .y = 0.0F, .z = std::sin(half_angle), .w = std::cos(half_angle)},
        .scale = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
    };

    const std::array<float, 16> matrix = to_model_matrix(transform);

    EXPECT_NEAR(matrix[0], 0.0F, 1e-6);
    EXPECT_NEAR(matrix[1], -1.0F, 1e-6);
    EXPECT_NEAR(matrix[4], 1.0F, 1e-6);
    EXPECT_NEAR(matrix[5], 0.0F, 1e-6);
    EXPECT_NEAR(matrix[10], 1.0F, 1e-6);
}

TEST(ToModelMatrix, RepeatedCallsWithIdenticalInputProduceBitIdenticalOutput) {
    const Transform transform{
        .position = {.x = 1.0F, .y = -2.0F, .z = 3.5F},
        .rotation = {.x = 0.1F, .y = 0.2F, .z = 0.3F, .w = 0.9F},
        .scale = {.x = 2.0F, .y = 0.5F, .z = 1.5F},
    };

    const std::array<float, 16> first = to_model_matrix(transform);
    const std::array<float, 16> second = to_model_matrix(transform);

    EXPECT_EQ(first, second);
}

} // namespace
} // namespace atlas::render
