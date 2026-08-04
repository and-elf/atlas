// Issue #182: proves resolve_camera_collision (demo/camera_collision.hpp)
// actually clamps a camera's desired position to real static geometry's
// surface - not merely "doesn't throw." Mirrors
// tests/atlas-physics/jolt_physics_backend_test.cpp's own
// SweepHitsRealStaticBoxBeforeReachingItsFarSide test setup directly (same
// box/tolerance shape) - this is exactly the same underlying query, now
// exercised through the demo-level helper rather than
// PhysicsBackend::sweep() directly. Only meaningful against the real
// JoltPhysicsBackend (NullPhysicsBackend::sweep() always returns
// std::nullopt, its own doc comment - there would be nothing to hit) -
// gated at the CMake level (demo/tests/CMakeLists.txt), mirroring
// jolt_physics_backend.hpp itself only being compiled/available when
// ATLAS_PHYSICS_BACKEND=JOLT (libraries/atlas-physics/CMakeLists.txt).
#include "atlas/physics/jolt_physics_backend.hpp"
#include "atlas/physics/null_physics_backend.hpp"

#include <gtest/gtest.h>

#include "camera_collision.hpp"

namespace atlas::demo {
namespace {

using atlas::physics::BodyCreateInfo;
using atlas::physics::BodyMotionType;
using atlas::physics::BoxShape;
using atlas::physics::JoltPhysicsBackend;
using atlas::physics::NullPhysicsBackend;

constexpr float box_half_extent = 1.0F;
constexpr float box_center_z_meters = 5.0F;
constexpr float box_near_face_z_meters = box_center_z_meters - box_half_extent; // 4.0F

// Jolt's own default contact/speculative-contact margin (JPH::
// PhysicsSettings::mPenetrationSlop/mSpeculativeContactDistance, both 0.02m
// by default) applies here exactly like it does to
// jolt_physics_backend_test.cpp's own sweep tests - this tolerance is,
// likewise, comfortably more than double that margin.
constexpr float tolerance = 0.05F;

TEST(CameraCollision, DesiredPositionBeyondAWallClampsToItsNearFace) {
    JoltPhysicsBackend backend;
    const atlas::physics::BodyId wall = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = box_center_z_meters},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = box_half_extent, .y = box_half_extent, .z = box_half_extent}},
    });
    (void)wall; // Only its position/shape matter to this test, not its id.

    const core::Vec3 pivot{.x = 0.0F, .y = 0.0F, .z = 0.0F};
    // Well past the wall's far face (box_center_z_meters + box_half_extent =
    // 6.0F) - proves the sweep stops at the near face rather than sailing
    // through to the desired end position.
    const core::Vec3 desired_position{.x = 0.0F, .y = 0.0F, .z = 10.0F};

    const core::Vec3 resolved = resolve_camera_collision(backend, pivot, desired_position);

    EXPECT_NEAR(resolved.x, 0.0F, tolerance);
    EXPECT_NEAR(resolved.y, 0.0F, tolerance);
    EXPECT_NEAR(resolved.z, box_near_face_z_meters, tolerance);
}

TEST(CameraCollision, DesiredPositionWithNothingInThePathIsReturnedUnchanged) {
    JoltPhysicsBackend backend; // An empty world - nothing created in it.

    const core::Vec3 pivot{.x = 0.0F, .y = 0.0F, .z = 0.0F};
    const core::Vec3 desired_position{.x = 0.0F, .y = 0.0F, .z = -10.0F};

    const core::Vec3 resolved = resolve_camera_collision(backend, pivot, desired_position);

    EXPECT_FLOAT_EQ(resolved.x, desired_position.x);
    EXPECT_FLOAT_EQ(resolved.y, desired_position.y);
    EXPECT_FLOAT_EQ(resolved.z, desired_position.z);
}

// Proves resolve_camera_collision is genuinely generic over any
// PhysicsBackend, not accidentally coupled to JoltPhysicsBackend's own
// concrete type - NullPhysicsBackend::sweep() always returns std::nullopt
// (its own doc comment), so desired_position is trivially always returned
// unchanged.
TEST(CameraCollision, NullBackendNeverCollidesSoDesiredPositionAlwaysReturnsUnchanged) {
    NullPhysicsBackend backend;

    const core::Vec3 pivot{.x = 0.0F, .y = 0.0F, .z = 0.0F};
    const core::Vec3 desired_position{.x = 1.0F, .y = 2.0F, .z = 3.0F};

    const core::Vec3 resolved = resolve_camera_collision(backend, pivot, desired_position);

    EXPECT_FLOAT_EQ(resolved.x, desired_position.x);
    EXPECT_FLOAT_EQ(resolved.y, desired_position.y);
    EXPECT_FLOAT_EQ(resolved.z, desired_position.z);
}

} // namespace
} // namespace atlas::demo
