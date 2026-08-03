#include "atlas/render/camera.hpp"

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace atlas::render {
namespace {

// This project's own established convention for designated-initializer
// aggregates (e.g. tests/atlas-render/sdl3_pixel_correctness_test.cpp's own
// Transform literals) lists every field explicitly rather than omitting
// some to fall back on default member initializers - -Wmissing-field-
// initializers (this project's own -Werror gate) flags a partial designated
// initializer, so every Camera literal below spells out all six fields, even
// when a given test only cares about one or two of them. These two matching
// the header's own documented defaults exactly (camera.hpp) keep every test
// below that doesn't care about projection parameters behaving as if it had
// used Camera{} directly.
constexpr float default_vertical_fov_radians = std::numbers::pi_v<float> / 3.0F;
constexpr float default_aspect_ratio = 16.0F / 9.0F;
constexpr float default_near_clip = 0.1F;
constexpr float default_far_clip = 1000.0F;

// Multiplies a row-major 4x4 matrix by the homogeneous point (x, y, z, 1) -
// this test file's own independent verification helper (deliberately not
// reusing camera.hpp's own detail::multiply_matrices, so a bug in that
// shared helper couldn't silently hide behind these tests too).
std::array<float, 4> transform_point(const std::array<float, 16>& matrix, float x, float y, float z) {
    std::array<float, 4> result{};
    const std::array<float, 4> point{x, y, z, 1.0F};
    for (std::size_t row = 0; row < 4; ++row) {
        float sum = 0.0F;
        for (std::size_t col = 0; col < 4; ++col) {
            sum += matrix.at((row * 4) + col) * point.at(col);
        }
        result.at(row) = sum;
    }
    return result;
}

TEST(Camera, DefaultIsAtTheOriginWithIdentityOrientation) {
    const Camera camera{};

    EXPECT_FLOAT_EQ(camera.position.x, 0.0F);
    EXPECT_FLOAT_EQ(camera.position.y, 0.0F);
    EXPECT_FLOAT_EQ(camera.position.z, 0.0F);
    EXPECT_FLOAT_EQ(camera.orientation.x, 0.0F);
    EXPECT_FLOAT_EQ(camera.orientation.y, 0.0F);
    EXPECT_FLOAT_EQ(camera.orientation.z, 0.0F);
    EXPECT_FLOAT_EQ(camera.orientation.w, 1.0F);
    EXPECT_LT(camera.near_clip, camera.far_clip);
}

// to_view_matrix (issue #181): the identity camera (origin, identity
// orientation) is its own inverse - the view matrix is the identity matrix.
TEST(ToViewMatrix, IdentityCameraProducesTheIdentityMatrix) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> matrix = to_view_matrix(camera);

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

// The camera's own world position must always transform to the view-space
// origin - true by definition of "view space is centered on the camera" -
// this is exactly the invariant a "negate position and orientation
// independently" bug (the mistake this library's own camera.hpp doc comment
// warns against) would violate for any camera with a non-identity
// orientation, so this test uses one.
TEST(ToViewMatrix, TranslationOnlyMapsTheCamerasOwnPositionToTheViewSpaceOrigin) {
    const Camera camera{
        .position = {.x = 3.0F, .y = -2.0F, .z = 5.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> matrix = to_view_matrix(camera);
    const std::array<float, 4> transformed_camera_position =
        transform_point(matrix, camera.position.x, camera.position.y, camera.position.z);

    EXPECT_NEAR(transformed_camera_position.at(0), 0.0F, 1e-5);
    EXPECT_NEAR(transformed_camera_position.at(1), 0.0F, 1e-5);
    EXPECT_NEAR(transformed_camera_position.at(2), 0.0F, 1e-5);
    EXPECT_NEAR(transformed_camera_position.at(3), 1.0F, 1e-5);
}

// A translation-only camera (identity orientation) at position p simply
// subtracts p - the simplest, directly-verifiable case (no rotation
// involved yet).
TEST(ToViewMatrix, TranslationOnlyCameraSubtractsItsOwnPositionFromEveryPoint) {
    const Camera camera{
        .position = {.x = 3.0F, .y = -2.0F, .z = 5.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> matrix = to_view_matrix(camera);
    const std::array<float, 4> transformed = transform_point(matrix, 10.0F, 10.0F, 10.0F);

    EXPECT_NEAR(transformed.at(0), 7.0F, 1e-5);
    EXPECT_NEAR(transformed.at(1), 12.0F, 1e-5);
    EXPECT_NEAR(transformed.at(2), 5.0F, 1e-5);
}

// This is the exact case a "negate position and orientation independently"
// bug gets wrong: a camera rotated 90 degrees about Y, positioned away from
// the origin. A world point lying exactly along the camera's own local
// forward axis (+Z, per to_view_matrix's own doc comment), at a known
// distance d, must transform to view-space (0, 0, d) - both the rotation
// AND the translation must be correctly combined (R^T * (world - position),
// not R^T * world - position, nor world - R^T * position) for this to hold.
TEST(ToViewMatrix, RotatedAndTranslatedCameraMapsItsOwnForwardAxisPointToPositiveViewSpaceZ) {
    // 90 degrees about Y: (x=0, y=sin(45deg), z=0, w=cos(45deg)).
    const float half_angle = std::numbers::pi_v<float> / 4.0F;
    const Camera camera{
        .position = {.x = 10.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = std::sin(half_angle), .z = 0.0F, .w = std::cos(half_angle)},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    // Camera-local forward (0, 0, d) rotated 90 degrees about Y becomes
    // world-space (d, 0, 0) - the standard rotation of the +Z axis by +90
    // about +Y. Offset by the camera's own position (10, 0, 0): the world
    // point 5 units along the camera's own forward axis is world (15, 0, 0).
    constexpr float distance = 5.0F;
    const std::array<float, 16> matrix = to_view_matrix(camera);
    const std::array<float, 4> transformed =
        transform_point(matrix, camera.position.x + distance, 0.0F, 0.0F);

    EXPECT_NEAR(transformed.at(0), 0.0F, 1e-4);
    EXPECT_NEAR(transformed.at(1), 0.0F, 1e-4);
    EXPECT_NEAR(transformed.at(2), distance, 1e-4);
}

TEST(ToViewMatrix, RepeatedCallsWithIdenticalInputProduceBitIdenticalOutput) {
    const Camera camera{
        .position = {.x = 1.0F, .y = 2.0F, .z = 3.0F},
        .orientation = {.x = 0.1F, .y = 0.2F, .z = 0.3F, .w = 0.9F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> first = to_view_matrix(camera);
    const std::array<float, 16> second = to_view_matrix(camera);

    EXPECT_EQ(first, second);
}

// to_projection_matrix (issue #181): SDL_GPU's own documented NDC (SDL_gpu.h,
// "Coordinate System") puts Z in [0, 1] with 0 at the near plane - verified
// here directly rather than assumed, exactly the "verify it yourself" bar
// this issue set.
TEST(ToProjectionMatrix, NearPlanePointProjectsToNdcZOfZero) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = 1.0F,
        .far_clip = 10.0F,
    };

    const std::array<float, 16> matrix = to_projection_matrix(camera);
    const std::array<float, 4> clip = transform_point(matrix, 0.0F, 0.0F, camera.near_clip);
    const float ndc_z = clip.at(2) / clip.at(3);

    EXPECT_NEAR(ndc_z, 0.0F, 1e-5);
}

TEST(ToProjectionMatrix, FarPlanePointProjectsToNdcZOfOne) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = 1.0F,
        .far_clip = 10.0F,
    };

    const std::array<float, 16> matrix = to_projection_matrix(camera);
    const std::array<float, 4> clip = transform_point(matrix, 0.0F, 0.0F, camera.far_clip);
    const float ndc_z = clip.at(2) / clip.at(3);

    EXPECT_NEAR(ndc_z, 1.0F, 1e-5);
}

// clip.w must equal view-space Z directly (no sign flip) - the direct
// consequence of this library's forward == +Z convention (see
// to_projection_matrix's own doc comment): a visible point already has a
// positive view-space Z, so it already has a positive w.
TEST(ToProjectionMatrix, ClipSpaceWEqualsViewSpaceZ) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> matrix = to_projection_matrix(camera);
    const std::array<float, 4> clip = transform_point(matrix, 0.0F, 0.0F, 7.0F);

    EXPECT_FLOAT_EQ(clip.at(3), 7.0F);
}

// A 90-degree vertical FOV (tan(45 deg) == 1) with a 1:1 aspect ratio gives
// xScale == yScale == 1 - the simplest case to verify the frustum's edge
// lands exactly at NDC (+-1, +-1) at a given depth, independent of the
// near/far-plane depth-remapping math the two tests above already cover.
TEST(ToProjectionMatrix, NinetyDegreeFovWithUnitAspectMapsTheFrustumEdgeToNdcUnity) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = std::numbers::pi_v<float> / 2.0F,
        .aspect_ratio = 1.0F,
        .near_clip = 1.0F,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> matrix = to_projection_matrix(camera);
    // At view-space depth z, the 90-degree half-frustum edge is exactly at
    // x = y = z (tan(45 deg) == 1).
    constexpr float depth = 3.0F;
    const std::array<float, 4> clip = transform_point(matrix, depth, depth, depth);

    EXPECT_NEAR(clip.at(0) / clip.at(3), 1.0F, 1e-5);
    EXPECT_NEAR(clip.at(1) / clip.at(3), 1.0F, 1e-5);
}

TEST(ToProjectionMatrix, RepeatedCallsWithIdenticalInputProduceBitIdenticalOutput) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> first = to_projection_matrix(camera);
    const std::array<float, 16> second = to_projection_matrix(camera);

    EXPECT_EQ(first, second);
}

// to_view_projection_matrix (issue #181): composes Projection * View -
// verified here against the identity-camera case, where the combined matrix
// must equal the projection matrix alone (View is the identity).
TEST(ToViewProjectionMatrix, IdentityCameraProducesTheProjectionMatrixAlone) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> combined = to_view_projection_matrix(camera);
    const std::array<float, 16> projection_only = to_projection_matrix(camera);

    for (std::size_t i = 0; i < combined.size(); ++i) {
        EXPECT_FLOAT_EQ(combined.at(i), projection_only.at(i)) << "index " << i;
    }
}

// A translated (but not rotated) camera composes View's translation with
// Projection's scale/depth-remap correctly - a world point at the camera's
// own forward-axis distance (near_clip, offset by the camera's position)
// must still land at NDC Z == 0, exactly the same invariant
// ToProjectionMatrix.NearPlanePointProjectsToNdcZOfZero already checks, but
// now through the full composed matrix rather than Projection alone.
TEST(ToViewProjectionMatrix, TranslatedCameraStillMapsItsOwnNearPlaneToNdcZOfZero) {
    const Camera camera{
        .position = {.x = 0.0F, .y = 0.0F, .z = 100.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = 1.0F,
        .far_clip = 10.0F,
    };

    const std::array<float, 16> matrix = to_view_projection_matrix(camera);
    const std::array<float, 4> clip =
        transform_point(matrix, camera.position.x, camera.position.y, camera.position.z + camera.near_clip);
    const float ndc_z = clip.at(2) / clip.at(3);

    EXPECT_NEAR(ndc_z, 0.0F, 1e-4);
}

TEST(ToViewProjectionMatrix, RepeatedCallsWithIdenticalInputProduceBitIdenticalOutput) {
    const Camera camera{
        .position = {.x = 1.0F, .y = 2.0F, .z = 3.0F},
        .orientation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
        .vertical_fov_radians = default_vertical_fov_radians,
        .aspect_ratio = default_aspect_ratio,
        .near_clip = default_near_clip,
        .far_clip = default_far_clip,
    };

    const std::array<float, 16> first = to_view_projection_matrix(camera);
    const std::array<float, 16> second = to_view_projection_matrix(camera);

    EXPECT_EQ(first, second);
}

} // namespace
} // namespace atlas::render
