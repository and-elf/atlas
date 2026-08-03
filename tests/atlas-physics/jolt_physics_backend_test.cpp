#include "atlas/physics/jolt_physics_backend.hpp"
#include "atlas/physics/physics_backend.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

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

} // namespace
} // namespace atlas::physics
