#include "atlas/physics/jolt_physics_backend.hpp"
#include "atlas/physics/physics_backend.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace atlas::physics {
namespace {

static_assert(PhysicsBackend<JoltPhysicsBackend>);

constexpr float gravity_meters_per_second_squared = 9.81F;
constexpr float time_step_seconds = 1.0F / 60.0F;
constexpr int steps = 60; // 1 second of simulated time at time_step_seconds.
constexpr float elapsed_seconds = time_step_seconds * static_cast<float>(steps);
constexpr float start_height_meters = 10.0F;

// Continuous free-fall kinematics (distance = 0.5 * g * t^2) as the
// reference estimate. A real semi-implicit-Euler integrator (what Jolt, like
// most real-time physics engines, uses) integrates velocity a half-step
// ahead of position and so overshoots this estimate slightly, while Jolt's
// own default per-body linear damping (5%/s, JPH::BodyCreationSettings::
// mLinearDamping) pulls the result back down slightly - rather than trying
// to reproduce Jolt's own integrator bit-for-bit, this test asserts a
// generously wide band around the continuous-kinematics estimate: tight
// enough to prove genuine gravity-driven integration happened (ruling out
// "didn't move at all" or "moved by some unrelated, coincidental amount"),
// wide enough to tolerate integrator/damping differences this test
// deliberately does not try to model exactly.
constexpr float expected_fall_meters =
    0.5F * gravity_meters_per_second_squared * elapsed_seconds * elapsed_seconds;
constexpr float min_plausible_fall_meters = expected_fall_meters * 0.5F;
constexpr float max_plausible_fall_meters = expected_fall_meters * 1.5F;

// Steps `backend` forward `steps_count` times at this file's own fixed
// time_step_seconds - factored out purely to keep the TEST bodies below
// under readability-function-cognitive-complexity's threshold (a bare for
// loop inline contributes real, load-bearing nesting to every assertion
// after it); it never sources delta_seconds internally itself, mirroring
// every PhysicsBackend's own discipline.
void step_n(JoltPhysicsBackend& backend, int steps_count) {
    for (int i = 0; i < steps_count; ++i) {
        backend.step(time_step_seconds);
    }
}

TEST(JoltPhysicsBackend, DynamicBodyFallsUnderGravityWithinPlausibleRange) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 0.0F, .y = start_height_meters, .z = 0.0F},
        .rotation = {},
    });

    for (int i = 0; i < steps; ++i) {
        backend.step(time_step_seconds);
    }

    const std::optional<BodyState> state = backend.body_state(body);
    ASSERT_TRUE(state.has_value());

    const float fallen_meters = start_height_meters - state->position.y;
    EXPECT_GT(fallen_meters, min_plausible_fall_meters);
    EXPECT_LT(fallen_meters, max_plausible_fall_meters);
}

TEST(JoltPhysicsBackend, StaticBodyDoesNotFall) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = start_height_meters, .z = 0.0F},
        .rotation = {},
    });

    for (int i = 0; i < steps; ++i) {
        backend.step(time_step_seconds);
    }

    const std::optional<BodyState> state = backend.body_state(body);
    ASSERT_TRUE(state.has_value());
    EXPECT_FLOAT_EQ(state->position.y, start_height_meters);
}

TEST(JoltPhysicsBackend, MultipleBodiesGetIndependentState) {
    JoltPhysicsBackend backend;
    const BodyId dynamic_body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 0.0F, .y = start_height_meters, .z = 0.0F},
        .rotation = {},
    });
    const BodyId static_body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 5.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {},
    });

    for (int i = 0; i < steps; ++i) {
        backend.step(time_step_seconds);
    }

    const std::optional<BodyState> dynamic_state = backend.body_state(dynamic_body);
    const std::optional<BodyState> static_state = backend.body_state(static_body);
    ASSERT_TRUE(dynamic_state.has_value());
    ASSERT_TRUE(static_state.has_value());

    EXPECT_LT(dynamic_state->position.y, start_height_meters);
    EXPECT_FLOAT_EQ(static_state->position.x, 5.0F);
    EXPECT_FLOAT_EQ(static_state->position.y, 0.0F);
}

TEST(JoltPhysicsBackend, CreatedBodyIsNeverNull) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{});

    EXPECT_FALSE(body.is_null());
}

TEST(JoltPhysicsBackend, DestroyBodyThenQueryReturnsNullopt) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{});

    backend.destroy_body(body);

    EXPECT_FALSE(backend.body_state(body).has_value());
}

TEST(JoltPhysicsBackend, DestroyingOneBodyDoesNotAffectAnother) {
    JoltPhysicsBackend backend;
    const BodyId first = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 1.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {},
    });
    const BodyId second = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 2.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {},
    });

    backend.destroy_body(first);

    EXPECT_FALSE(backend.body_state(first).has_value());
    ASSERT_TRUE(backend.body_state(second).has_value());
    EXPECT_FLOAT_EQ(backend.body_state(second)->position.x, 2.0F);
}

TEST(JoltPhysicsBackend, DestroyingAnAlreadyDestroyedBodyIsANoOp) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{});

    backend.destroy_body(body);
    backend.destroy_body(body);

    EXPECT_FALSE(backend.body_state(body).has_value());
}

TEST(JoltPhysicsBackend, QueryingABodyIdThatWasNeverCreatedReturnsNullopt) {
    const JoltPhysicsBackend backend;
    const BodyId never_created{.index = 0, .generation = 0};

    EXPECT_FALSE(backend.body_state(never_created).has_value());
}

TEST(JoltPhysicsBackend, CreateBodyThrowsOnceThisInstancesBodyBudgetIsExhausted) {
    JoltPhysicsBackend backend;
    // Matches max_bodies in jolt_physics_backend.cpp exactly - every body
    // created here is Static (never activated, no simulation step run), so
    // this only exercises body/broadphase bookkeeping, not the simulation
    // loop.
    constexpr int max_bodies_budget = 1024;
    const BodyCreateInfo create_info{
        .motion_type = BodyMotionType::Static,
        .position = {},
        .rotation = {},
    };
    for (int i = 0; i < max_bodies_budget; ++i) {
        const BodyId body = backend.create_body(create_info);
        (void)body;
    }

    EXPECT_THROW(
        {
            const BodyId body = backend.create_body(create_info);
            (void)body;
        },
        std::runtime_error);
}

// --- Shape conversion (issue #179) -------------------------------------------

TEST(JoltPhysicsBackend, CreateBodyWithBoxShapeSucceeds) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = 1.0F, .y = 2.0F, .z = 3.0F}},
    });

    EXPECT_FALSE(body.is_null());
    EXPECT_TRUE(backend.body_state(body).has_value());
}

TEST(JoltPhysicsBackend, CreateBodyWithCapsuleShapeSucceeds) {
    JoltPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {},
        .shape = CapsuleShape{.half_height = 1.0F, .radius = 0.5F},
    });

    EXPECT_FALSE(body.is_null());
    EXPECT_TRUE(backend.body_state(body).has_value());
}

TEST(JoltPhysicsBackend, CreateBodyWithValidConvexHullShapeSucceeds) {
    JoltPhysicsBackend backend;
    // A tetrahedron - the simplest possible genuinely 3D convex hull.
    const BodyId body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
        .rotation = {},
        .shape = ConvexHullShape{.points = {{.x = 0.0F, .y = 0.0F, .z = 0.0F},
                                            {.x = 1.0F, .y = 0.0F, .z = 0.0F},
                                            {.x = 0.0F, .y = 1.0F, .z = 0.0F},
                                            {.x = 0.0F, .y = 0.0F, .z = 1.0F}}},
    });

    EXPECT_FALSE(body.is_null());
    EXPECT_TRUE(backend.body_state(body).has_value());
}

TEST(JoltPhysicsBackend, CreateBodyWithDegenerateConvexHullShapeThrows) {
    JoltPhysicsBackend backend;
    // No points at all cannot form a hull - JPH::ConvexHullShapeSettings::
    // Create() reports this as an error rather than crashing or silently
    // producing an empty shape; make_jolt_shape() surfaces that as a thrown
    // std::runtime_error, mirroring create_body()'s own body-budget-exhausted
    // convention.
    EXPECT_THROW(
        {
            const BodyId body = backend.create_body(BodyCreateInfo{
                .motion_type = BodyMotionType::Static,
                .position = {.x = 0.0F, .y = 0.0F, .z = 0.0F},
                .rotation = {},
                .shape = ConvexHullShape{.points = {}},
            });
            (void)body;
        },
        std::runtime_error);
}

TEST(JoltPhysicsBackend, DefaultShapeIsSphereMatchingIssue178sOwnPlaceholderRadius) {
    // BodyCreateInfo{} (no shape specified) must keep compiling and behaving
    // exactly like every pre-#179 call site - a 0.5m-radius SphereShape,
    // issue #178's own hardcoded placeholder radius.
    JoltPhysicsBackend backend;
    const BodyId dynamic_body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 0.0F, .y = start_height_meters, .z = 0.0F},
        .rotation = {},
    });

    for (int i = 0; i < steps; ++i) {
        backend.step(time_step_seconds);
    }

    const std::optional<BodyState> state = backend.body_state(dynamic_body);
    ASSERT_TRUE(state.has_value());
    EXPECT_LT(state->position.y, start_height_meters);
}

// --- Real collision resolution proof (issue #179) ---------------------------
//
// #178's own DynamicBodyFallsUnderGravityWithinPlausibleRange test proved
// gravity integration alone - its dynamic body just fell freely, with nothing
// to land on. This test proves the other half of "real rigid-body
// simulation": a Dynamic body colliding with, and coming to rest on, a
// Static body's real shape - genuinely resolved contact response, not merely
// "doesn't throw."
//
// The floor is a wide, flat BoxShape (its half-extents span 10m x 0.5m x 10m)
// centered at floor_center_y so its top surface sits at exactly
// floor_top_y_meters (0.0m - a deliberately round number). The falling body
// is a SphereShape of radius falling_body_radius_meters, dropped from
// drop_start_y_meters directly above the floor's center, with no initial
// velocity - so once it settles, its own center's Y position should stabilize
// at floor_top_y_meters + falling_body_radius_meters, the geometrically exact
// resting height for a sphere sitting on a flat plane.
namespace collision_resolution {

constexpr float floor_half_extent_y_meters = 0.5F;
constexpr float floor_center_y_meters = -0.5F;
constexpr float floor_top_y_meters = floor_center_y_meters + floor_half_extent_y_meters; // 0.0F
constexpr float falling_body_radius_meters = 0.5F;
constexpr float expected_resting_y_meters = floor_top_y_meters + falling_body_radius_meters; // 0.5F
constexpr float drop_start_y_meters = 3.0F;

// 3s of simulated time at time_step_seconds - generous for a 2.5m drop
// (continuous free-fall kinematics alone predicts landing in well under 1s),
// leaving ample budget for Jolt's own contact resolution/restitution damping
// to fully settle before this test ever queries the body's state.
constexpr int settle_steps = 180;

// A further 1s of simulated time, stepped *after* the settling check below -
// this is what actually distinguishes "genuinely came to rest" from "merely
// hasn't fallen through yet at this particular step count": if collision
// resolution were subtly broken (e.g. the floor's shape not truly being
// collided against), the body would keep sinking across these extra steps
// instead of staying put.
constexpr int extra_steps_after_settling = 60;

// A generous band around the geometrically exact resting height, to absorb
// Jolt's own real contact-resolution slop (JPH::PhysicsSettings::
// mPenetrationSlop/mSpeculativeContactDistance both default to 0.02m) without
// masking a genuine "fell through" or "never landed" failure - this test's
// own settling tolerance is more than double that combined slop.
constexpr float resting_tolerance_meters = 0.05F;

// Once genuinely settled (and, per mAllowSleeping's own default, likely
// asleep), further steps should move this body by only a hair - far tighter
// than resting_tolerance_meters above, since this checks "stayed put," not
// "landed in the right place."
constexpr float post_settle_drift_tolerance_meters = 0.01F;

} // namespace collision_resolution

TEST(JoltPhysicsBackend, DynamicBodySettlesOnStaticFloorAndDoesNotFallThrough) {
    using namespace collision_resolution;

    JoltPhysicsBackend backend;

    const BodyId floor = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = floor_center_y_meters, .z = 0.0F},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = 10.0F, .y = floor_half_extent_y_meters, .z = 10.0F}},
    });
    const BodyId falling_body = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 0.0F, .y = drop_start_y_meters, .z = 0.0F},
        .rotation = {},
        .shape = SphereShape{.radius = falling_body_radius_meters},
    });

    step_n(backend, settle_steps);

    const std::optional<BodyState> settled_state = backend.body_state(falling_body);
    ASSERT_TRUE(settled_state.has_value());
    EXPECT_NEAR(settled_state->position.y, expected_resting_y_meters, resting_tolerance_meters);

    step_n(backend, extra_steps_after_settling);

    const std::optional<BodyState> post_settle_state = backend.body_state(falling_body);
    ASSERT_TRUE(post_settle_state.has_value());
    EXPECT_NEAR(post_settle_state->position.y, settled_state->position.y, post_settle_drift_tolerance_meters);

    // The floor itself is Static - confirm it genuinely never moved either.
    const std::optional<BodyState> floor_state = backend.body_state(floor);
    ASSERT_TRUE(floor_state.has_value());
    EXPECT_FLOAT_EQ(floor_state->position.y, floor_center_y_meters);
}

// --- Bit-exact determinism proof (issue #179, §4) ---------------------------
//
// §4's bit-exact determinism guarantee applied to atlas-physics for the
// first time: identical body setup, stepped an identical number of times
// with an identical fixed timestep, on two entirely separate
// JoltPhysicsBackend instances, must produce bit-for-bit identical resulting
// BodyState for every body - exact `==`/EXPECT_EQ on the raw float
// components, deliberately never EXPECT_NEAR/EXPECT_FLOAT_EQ (which would
// mask a real determinism violation behind a tolerance).
namespace determinism {

// A small mixed scene (one Static floor, two Dynamic bodies of different
// shapes with different starting positions) - enough real collision/contact
// activity for a genuine nondeterminism (were one present) to have a chance
// to surface, not just two bodies falling in isolation. Always stepped at
// this file's own fixed time_step_seconds (step_n) - a PhysicsBackend must
// never source its own timestep, so there is nothing for a second parameter
// to meaningfully vary here.
std::vector<BodyState> run_scenario(int steps_to_run) {
    JoltPhysicsBackend backend;

    const BodyId floor = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = -0.5F, .z = 0.0F},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = 10.0F, .y = 0.5F, .z = 10.0F}},
    });
    const BodyId sphere = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 0.0F, .y = 3.0F, .z = 0.25F},
        .rotation = {},
        .shape = SphereShape{.radius = 0.5F},
    });
    const BodyId box = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 2.0F, .y = 5.0F, .z = -1.0F},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = 0.5F, .y = 0.5F, .z = 0.5F}},
    });

    step_n(backend, steps_to_run);

    return {backend.body_state(floor).value(),
            backend.body_state(sphere).value(),
            backend.body_state(box).value()};
}

// Asserts every raw float component of two BodyStates is bit-for-bit
// identical - exact `==`/EXPECT_EQ throughout, deliberately never
// EXPECT_NEAR/EXPECT_FLOAT_EQ (§4's own bit-exact determinism guarantee would
// be meaningless if checked with a tolerance). Split into position/rotation
// halves (each called once from expect_bit_exact() below) purely to keep
// every function here under readability-function-cognitive-complexity's
// threshold - gtest's own EXPECT_EQ macro expansion (a nested switch/if/else)
// inflates this metric well beyond what these assertions' actual, purely
// sequential, non-branching logic warrants; splitting is a real fix rather
// than a NOLINT, since each half is still a genuinely coherent, independently
// readable unit ("do the position fields match" / "do the rotation fields
// match").
void expect_position_bit_exact(const core::Vec3& first, const core::Vec3& second, const char* body_label) {
    EXPECT_EQ(first.x, second.x) << body_label << " position.x diverged";
    EXPECT_EQ(first.y, second.y) << body_label << " position.y diverged";
    EXPECT_EQ(first.z, second.z) << body_label << " position.z diverged";
}

void expect_rotation_bit_exact(const core::Quaternion& first,
                               const core::Quaternion& second,
                               const char* body_label) {
    EXPECT_EQ(first.x, second.x) << body_label << " rotation.x diverged";
    EXPECT_EQ(first.y, second.y) << body_label << " rotation.y diverged";
    EXPECT_EQ(first.z, second.z) << body_label << " rotation.z diverged";
    EXPECT_EQ(first.w, second.w) << body_label << " rotation.w diverged";
}

void expect_bit_exact(const BodyState& first, const BodyState& second, const char* body_label) {
    expect_position_bit_exact(first.position, second.position, body_label);
    expect_rotation_bit_exact(first.rotation, second.rotation, body_label);
}

} // namespace determinism

TEST(JoltPhysicsBackend, IdenticalSetupAndStepsProduceBitExactIdenticalState) {
    constexpr int steps_to_run = 120; // 2s of simulated time - long enough to include real contacts.
    constexpr std::size_t expected_body_count = 3; // floor, sphere, box - see run_scenario().

    const std::vector<BodyState> run1 = determinism::run_scenario(steps_to_run);
    const std::vector<BodyState> run2 = determinism::run_scenario(steps_to_run);

    ASSERT_EQ(run1.size(), expected_body_count);
    ASSERT_EQ(run2.size(), expected_body_count);
    determinism::expect_bit_exact(run1[0], run2[0], "floor");
    determinism::expect_bit_exact(run1[1], run2[1], "sphere");
    determinism::expect_bit_exact(run1[2], run2[2], "box");
}

} // namespace
} // namespace atlas::physics
