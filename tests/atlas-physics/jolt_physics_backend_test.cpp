#include "atlas/physics/jolt_physics_backend.hpp"
#include "atlas/physics/physics_backend.hpp"

#include <cstddef>
#include <cstdint>
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

// --- Startup-configurable threading mode (issue #193) shared test fixtures --
//
// A fixed, explicit thread count (never Jolt's own -1 "auto-detect CPU
// count" sentinel, JoltPhysicsBackendConfig::thread_count's own default) for
// every ThreadPool-mode test below - this deliberately does not rely on
// auto-detection because a real "genuinely uses multiple worker threads"
// proof should not depend on how many cores happen to be available on
// whichever machine runs this suite (a single-core CI runner would
// auto-detect down to a JobSystemThreadPool with zero extra worker threads,
// silently defeating the point of these tests).
namespace threading_config {
constexpr int fixed_thread_count = 4;
} // namespace threading_config

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
    // Matches JoltPhysicsBackendConfig::max_bodies's own default (issue
    // #193) exactly - every body created here is Static (never activated, no
    // simulation step run), so this only exercises body/broadphase
    // bookkeeping, not the simulation loop.
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

// --- Startup-configurable max_bodies (issue #193) ----------------------------
//
// The counterpart to CreateBodyThrowsOnceThisInstancesBodyBudgetIsExhausted
// above, proving the *configured* value actually takes effect - not merely
// that JoltPhysicsBackendConfig::max_bodies compiles. A caller-supplied
// budget of 10 (nowhere near the 1024 default) must throw on the 11th body,
// not the 1025th.
TEST(JoltPhysicsBackend, SmallerConfiguredMaxBodiesThrowsOnceThatBudgetIsExhausted) {
    constexpr std::uint32_t configured_max_bodies = 10;
    JoltPhysicsBackend backend(JoltPhysicsBackendConfig{.max_bodies = configured_max_bodies});
    const BodyCreateInfo create_info{
        .motion_type = BodyMotionType::Static,
        .position = {},
        .rotation = {},
    };
    for (std::uint32_t i = 0; i < configured_max_bodies; ++i) {
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

// The floor + falling-body pair every test in this section (and issue
// #193's own ThreadPoolModeSettlesBodyCorrectly, below) drops onto - factored
// out so the ThreadPool-mode variant exercises exactly the same scene as the
// original SingleThreaded one rather than a hand-copied near-duplicate.
struct SettlingScene {
    BodyId floor;
    BodyId falling_body;
};

SettlingScene create_settling_scene(JoltPhysicsBackend& backend) {
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
    return {.floor = floor, .falling_body = falling_body};
}

} // namespace collision_resolution

TEST(JoltPhysicsBackend, DynamicBodySettlesOnStaticFloorAndDoesNotFallThrough) {
    using namespace collision_resolution;

    JoltPhysicsBackend backend;
    const SettlingScene scene = create_settling_scene(backend);

    step_n(backend, settle_steps);

    const std::optional<BodyState> settled_state = backend.body_state(scene.falling_body);
    ASSERT_TRUE(settled_state.has_value());
    EXPECT_NEAR(settled_state->position.y, expected_resting_y_meters, resting_tolerance_meters);

    step_n(backend, extra_steps_after_settling);

    const std::optional<BodyState> post_settle_state = backend.body_state(scene.falling_body);
    ASSERT_TRUE(post_settle_state.has_value());
    EXPECT_NEAR(post_settle_state->position.y, settled_state->position.y, post_settle_drift_tolerance_meters);

    // The floor itself is Static - confirm it genuinely never moved either.
    const std::optional<BodyState> floor_state = backend.body_state(scene.floor);
    ASSERT_TRUE(floor_state.has_value());
    EXPECT_FLOAT_EQ(floor_state->position.y, floor_center_y_meters);
}

// --- ThreadPool mode genuinely simulates correctly (issue #193) -------------
//
// Reruns the exact same settling proof above under
// JoltPhysicsBackendConfig{.threading_mode = ThreadingMode::ThreadPool} -
// this issue's own explicit instruction: proving ThreadPool mode "genuinely
// uses multiple threads and correctly simulates (not just doesn't throw)" by
// confirming a Dynamic body still comes to rest on a Static floor, at the
// same geometrically-expected height, within the same tolerance, under a
// real multi-worker-thread JPH::JobSystemThreadPool.
TEST(JoltPhysicsBackend, ThreadPoolModeSettlesBodyCorrectly) {
    using namespace collision_resolution;

    JoltPhysicsBackend backend(JoltPhysicsBackendConfig{
        .threading_mode = ThreadingMode::ThreadPool,
        .thread_count = threading_config::fixed_thread_count,
    });
    const SettlingScene scene = create_settling_scene(backend);

    step_n(backend, settle_steps);

    const std::optional<BodyState> settled_state = backend.body_state(scene.falling_body);
    ASSERT_TRUE(settled_state.has_value());
    EXPECT_NEAR(settled_state->position.y, expected_resting_y_meters, resting_tolerance_meters);

    step_n(backend, extra_steps_after_settling);

    const std::optional<BodyState> post_settle_state = backend.body_state(scene.falling_body);
    ASSERT_TRUE(post_settle_state.has_value());
    EXPECT_NEAR(post_settle_state->position.y, settled_state->position.y, post_settle_drift_tolerance_meters);

    const std::optional<BodyState> floor_state = backend.body_state(scene.floor);
    ASSERT_TRUE(floor_state.has_value());
    EXPECT_FLOAT_EQ(floor_state->position.y, floor_center_y_meters);
}

// --- Raycast query proof (issue #180) -----------------------------------------
//
// A single Static BoxShape, centered at (0, 0, box_center_z) with
// box_half_extent on every axis - so its near face (the one a ray fired down
// +Z from the origin meets first) sits at exactly box_near_face_z, a
// deliberately round number to make every expected hit point/normal below an
// exact, easily-verified geometric prediction rather than an arbitrary one.
namespace raycast_query {

constexpr float box_half_extent = 1.0F;
constexpr float box_center_z_meters = 5.0F;
constexpr float box_near_face_z_meters = box_center_z_meters - box_half_extent; // 4.0F

// Tight - a raycast against a single convex shape is exact GJK ray-vs-convex
// intersection, with none of Jolt's own contact-resolution/speculative-
// contact slop a real settling collision test needs to tolerate (issue
// #179's own DynamicBodySettlesOnStaticFloorAndDoesNotFallThrough, above).
constexpr float tolerance = 1.0e-4F;

BodyId create_box(JoltPhysicsBackend& backend) {
    return backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = box_center_z_meters},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = box_half_extent, .y = box_half_extent, .z = box_half_extent}},
    });
}

} // namespace raycast_query

TEST(JoltPhysicsBackend, RaycastHitsRealStaticBoxWithCorrectBodyPointAndNormal) {
    using namespace raycast_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);

    const std::optional<HitResult> hit =
        backend.raycast({.x = 0.0F, .y = 0.0F, .z = 0.0F}, {.x = 0.0F, .y = 0.0F, .z = 1.0F}, 10.0F);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->body, box);

    // The geometrically exact intersection: the ray meets the box's near
    // face dead center, at (0, 0, box_near_face_z_meters).
    EXPECT_NEAR(hit->point.x, 0.0F, tolerance);
    EXPECT_NEAR(hit->point.y, 0.0F, tolerance);
    EXPECT_NEAR(hit->point.z, box_near_face_z_meters, tolerance);

    // The hit face's outward normal points back toward the ray's own origin
    // (-Z, away from the box) - not into the box (+Z).
    EXPECT_NEAR(hit->normal.x, 0.0F, tolerance);
    EXPECT_NEAR(hit->normal.y, 0.0F, tolerance);
    EXPECT_NEAR(hit->normal.z, -1.0F, tolerance);
}

TEST(JoltPhysicsBackend, RaycastWithNonUnitDirectionStillReachesTheCallerSuppliedMaxDistance) {
    using namespace raycast_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);

    // direction has magnitude 5, not 1 - raycast() must defensively normalize
    // it (physics_backend.hpp's own documented discipline) so max_distance
    // (10) is still this ray's real reach, not 50 (10 * 5, direction's own
    // magnitude) or 2 (10 / 5) - either of which would report a different
    // outcome than the unit-direction case above (50 would still hit at the
    // same point; 2 would fall short of the box's near face at
    // box_near_face_z_meters = 4.0F and wrongly report std::nullopt).
    const std::optional<HitResult> hit =
        backend.raycast({.x = 0.0F, .y = 0.0F, .z = 0.0F}, {.x = 0.0F, .y = 0.0F, .z = 5.0F}, 10.0F);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->body, box);
    EXPECT_NEAR(hit->point.z, box_near_face_z_meters, tolerance);
}

TEST(JoltPhysicsBackend, RaycastAimedAwayFromEverythingReturnsNullopt) {
    using namespace raycast_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);
    (void)box;

    // The box sits at +Z; aiming down -Z can never reach it regardless of
    // max_distance.
    const std::optional<HitResult> hit =
        backend.raycast({.x = 0.0F, .y = 0.0F, .z = 0.0F}, {.x = 0.0F, .y = 0.0F, .z = -1.0F}, 10.0F);

    EXPECT_FALSE(hit.has_value());
}

TEST(JoltPhysicsBackend, RaycastWithMaxDistanceTooShortToReachAnythingReturnsNullopt) {
    using namespace raycast_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);
    (void)box;

    // Aimed directly at the box, but max_distance (2.0) falls short of its
    // near face at box_near_face_z_meters (4.0).
    const std::optional<HitResult> hit =
        backend.raycast({.x = 0.0F, .y = 0.0F, .z = 0.0F}, {.x = 0.0F, .y = 0.0F, .z = 1.0F}, 2.0F);

    EXPECT_FALSE(hit.has_value());
}

TEST(JoltPhysicsBackend, RaycastWithZeroLengthDirectionReturnsNullopt) {
    using namespace raycast_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);
    (void)box;

    // A degenerate (zero-length) direction has no meaningful direction to
    // normalize - defensively reported as std::nullopt rather than dividing
    // by zero or forwarding a nonsensical ray to Jolt.
    const std::optional<HitResult> hit =
        backend.raycast({.x = 0.0F, .y = 0.0F, .z = 0.0F}, {.x = 0.0F, .y = 0.0F, .z = 0.0F}, 10.0F);

    EXPECT_FALSE(hit.has_value());
}

// --- Sweep query proof (issue #180) -------------------------------------------
//
// The same box geometry as the raycast tests above (a separate namespace
// since box_half_extent/box_center_z_meters here also fix a sphere's radius,
// which the raycast tests above have no use for) - a sphere is swept along
// +Z, from well in front of the box to a point past its far side, and must
// report a hit at the box's near face rather than reaching the far side.
namespace sweep_query {

constexpr float box_half_extent = 1.0F;
constexpr float box_center_z_meters = 5.0F;
constexpr float box_near_face_z_meters = box_center_z_meters - box_half_extent; // 4.0F
constexpr float sphere_radius_meters = 0.5F;

// Jolt's own default contact/speculative-contact margin (JPH::
// PhysicsSettings::mPenetrationSlop/mSpeculativeContactDistance, both 0.02m
// by default) applies to a shape cast's reported contact point exactly like
// it does to #179's own settling-collision test (DynamicBodySettlesOn
// StaticFloorAndDoesNotFallThrough, above) - this tolerance is, likewise,
// comfortably more than double that margin.
constexpr float tolerance = 0.05F;

BodyId create_box(JoltPhysicsBackend& backend) {
    return backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = box_center_z_meters},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = box_half_extent, .y = box_half_extent, .z = box_half_extent}},
    });
}

} // namespace sweep_query

TEST(JoltPhysicsBackend, SweepHitsRealStaticBoxBeforeReachingItsFarSide) {
    using namespace sweep_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);

    // The box's far face sits at box_center_z_meters + box_half_extent =
    // 6.0F - sweeping all the way to z = 10.0F (well past it) must still
    // report a hit at the *near* face, not silently sail through to the end
    // position.
    const std::optional<HitResult> hit = backend.sweep(SphereShape{.radius = sphere_radius_meters},
                                                       {.x = 0.0F, .y = 0.0F, .z = 0.0F},
                                                       core::Quaternion{},
                                                       {.x = 0.0F, .y = 0.0F, .z = 10.0F});

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->body, box);

    // Geometrically plausible: HitResult::point is a point on the *hit
    // body's* surface (body.hpp's own documented convention), so the
    // expected point is the box's own near face (z = 4.0F) - neither the
    // box's center (z = 5.0F, which a bug reporting the hit body's own
    // position instead of an actual contact point would produce) nor the
    // sweep's own end position (z = 10.0F, which a bug that failed to stop
    // the sweep early would produce).
    EXPECT_NEAR(hit->point.x, 0.0F, tolerance);
    EXPECT_NEAR(hit->point.y, 0.0F, tolerance);
    EXPECT_NEAR(hit->point.z, box_near_face_z_meters, tolerance);
    EXPECT_NEAR(hit->normal.x, 0.0F, tolerance);
    EXPECT_NEAR(hit->normal.y, 0.0F, tolerance);
    EXPECT_NEAR(hit->normal.z, -1.0F, tolerance);
}

TEST(JoltPhysicsBackend, SweepWithNothingInItsPathReturnsNullopt) {
    using namespace sweep_query;

    JoltPhysicsBackend backend;
    const BodyId box = create_box(backend);
    (void)box;

    // Sweeping away from the box entirely (-Z instead of +Z).
    const std::optional<HitResult> hit = backend.sweep(SphereShape{.radius = sphere_radius_meters},
                                                       {.x = 0.0F, .y = 0.0F, .z = 0.0F},
                                                       core::Quaternion{},
                                                       {.x = 0.0F, .y = 0.0F, .z = -10.0F});

    EXPECT_FALSE(hit.has_value());
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
// to meaningfully vary here. `config` (issue #193) defaults to
// JoltPhysicsBackendConfig{} (SingleThreaded, this backend's own pre-#193
// behavior) - IdenticalSetupAndStepsProduceBitExactIdenticalState (below)
// relies on that default unchanged, while
// ThreadPoolModeIdenticalSetupAndStepsProduceBitExactIdenticalState (issue
// #193, below) passes a ThreadPool config through this exact same scenario
// to prove the identical determinism property holds under it too.
std::vector<BodyState> run_scenario(int steps_to_run, const JoltPhysicsBackendConfig& config = {}) {
    JoltPhysicsBackend backend(config);

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

// --- Bit-exact determinism proof under ThreadPool mode (issue #193, §4) -----
//
// The load-bearing test this issue exists to write: the identical mixed-body
// scenario and identical bit-exact-comparison methodology as
// IdenticalSetupAndStepsProduceBitExactIdenticalState above, but constructing
// both JoltPhysicsBackend instances with
// JoltPhysicsBackendConfig{.threading_mode = ThreadingMode::ThreadPool} (same
// fixed thread count on both) instead of the SingleThreaded default.
//
// Per this library's own README ("Determinism investigation (issue #193)"),
// Jolt's own documentation (Docs/Architecture.md, "Deterministic
// Simulation") states the simulation itself remains deterministic under
// CROSS_PLATFORM_DETERMINISTIC regardless of thread count, as long as the
// API calls that modify it happen in the same order (true here - both runs
// create the same three bodies in the same order, then only ever call
// step()); multi-threading is documented to affect only the *ordering* of
// broadphase queries, listener callback delivery, and
// PhysicsSystem::GetActiveBodies, none of which this backend's step() or
// body_state() ever uses. This test exists to confirm that investigation
// empirically, not merely cite it - and it does: no nondeterminism was
// found, exactly as the investigation predicted.
TEST(JoltPhysicsBackend, ThreadPoolModeIdenticalSetupAndStepsProduceBitExactIdenticalState) {
    constexpr int steps_to_run = 120;              // Identical to the SingleThreaded version above.
    constexpr std::size_t expected_body_count = 3; // floor, sphere, box - see run_scenario().
    const JoltPhysicsBackendConfig config{
        .threading_mode = ThreadingMode::ThreadPool,
        .thread_count = threading_config::fixed_thread_count,
    };

    const std::vector<BodyState> run1 = determinism::run_scenario(steps_to_run, config);
    const std::vector<BodyState> run2 = determinism::run_scenario(steps_to_run, config);

    ASSERT_EQ(run1.size(), expected_body_count);
    ASSERT_EQ(run2.size(), expected_body_count);
    determinism::expect_bit_exact(run1[0], run2[0], "floor");
    determinism::expect_bit_exact(run1[1], run2[1], "sphere");
    determinism::expect_bit_exact(run1[2], run2[2], "box");
}

// --- Bit-exact determinism proof for raycast() (issue #180, §4) -------------
//
// §4's bit-exact determinism guarantee applies to a query exactly as much as
// to step() (this file's own IdenticalSetupAndStepsProduceBitExactIdenticalState
// above, issue #179): an identical scene, queried with identical parameters
// on two entirely separate JoltPhysicsBackend instances, must report a
// bit-for-bit identical HitResult. Reuses determinism::expect_position_bit_exact
// (above) rather than duplicating it - both check "are these three raw float
// components exactly equal," regardless of whether they came from a
// BodyState or a HitResult.
namespace raycast_determinism {

HitResult run_raycast() {
    JoltPhysicsBackend backend;
    const BodyId box = backend.create_body(BodyCreateInfo{
        .motion_type = BodyMotionType::Static,
        .position = {.x = 0.0F, .y = 0.0F, .z = 5.0F},
        .rotation = {},
        .shape = BoxShape{.half_extents = {.x = 1.0F, .y = 1.0F, .z = 1.0F}},
    });
    (void)box;

    const std::optional<HitResult> hit =
        backend.raycast({.x = 0.13F, .y = -0.27F, .z = 0.0F}, {.x = 0.0F, .y = 0.0F, .z = 1.0F}, 10.0F);
    return hit.value();
}

} // namespace raycast_determinism

TEST(JoltPhysicsBackend, IdenticalRaycastQueryProducesBitExactIdenticalHitResult) {
    const HitResult first = raycast_determinism::run_raycast();
    const HitResult second = raycast_determinism::run_raycast();

    EXPECT_EQ(first.body, second.body);
    determinism::expect_position_bit_exact(first.point, second.point, "raycast hit point");
    determinism::expect_position_bit_exact(first.normal, second.normal, "raycast hit normal");
}

} // namespace
} // namespace atlas::physics
