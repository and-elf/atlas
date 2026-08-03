#include "atlas/physics/null_physics_backend.hpp"
#include "atlas/physics/physics_backend.hpp"

#include <gtest/gtest.h>

namespace atlas::physics {
namespace {

static_assert(PhysicsBackend<NullPhysicsBackend>);

TEST(NullPhysicsBackend, CreateBodyThenQueryReturnsTheExactCreatedState) {
    NullPhysicsBackend backend;
    const BodyCreateInfo create_info{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 1.0F, .y = 2.0F, .z = 3.0F},
        .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
    };

    const BodyId body = backend.create_body(create_info);
    const std::optional<BodyState> state = backend.body_state(body);

    ASSERT_TRUE(state.has_value());
    EXPECT_FLOAT_EQ(state->position.x, 1.0F);
    EXPECT_FLOAT_EQ(state->position.y, 2.0F);
    EXPECT_FLOAT_EQ(state->position.z, 3.0F);
    EXPECT_FLOAT_EQ(state->rotation.w, 1.0F);
}

TEST(NullPhysicsBackend, CreatedBodyIsNeverNull) {
    NullPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{});

    EXPECT_FALSE(body.is_null());
}

TEST(NullPhysicsBackend, DestroyBodyThenQueryReturnsNullopt) {
    NullPhysicsBackend backend;
    const BodyId body = backend.create_body(BodyCreateInfo{});

    backend.destroy_body(body);

    EXPECT_FALSE(backend.body_state(body).has_value());
}

TEST(NullPhysicsBackend, StepDoesNotChangeAnyBodysState) {
    NullPhysicsBackend backend;
    const BodyCreateInfo create_info{
        .motion_type = BodyMotionType::Dynamic,
        .position = {.x = 5.0F, .y = -1.0F, .z = 0.5F},
        .rotation = {.x = 0.1F, .y = 0.2F, .z = 0.3F, .w = 0.9F},
    };
    const BodyId body = backend.create_body(create_info);

    backend.step(1.0F / 60.0F);
    backend.step(1.0F / 60.0F);

    const std::optional<BodyState> state = backend.body_state(body);
    ASSERT_TRUE(state.has_value());
    EXPECT_FLOAT_EQ(state->position.x, 5.0F);
    EXPECT_FLOAT_EQ(state->position.y, -1.0F);
    EXPECT_FLOAT_EQ(state->position.z, 0.5F);
    EXPECT_FLOAT_EQ(state->rotation.x, 0.1F);
    EXPECT_FLOAT_EQ(state->rotation.y, 0.2F);
    EXPECT_FLOAT_EQ(state->rotation.z, 0.3F);
    EXPECT_FLOAT_EQ(state->rotation.w, 0.9F);
}

TEST(NullPhysicsBackend, MultipleCreatedBodiesGetDistinctIdsAndIndependentState) {
    NullPhysicsBackend backend;
    const BodyId first = backend.create_body(BodyCreateInfo{.motion_type = BodyMotionType::Static,
                                                            .position = {.x = 1.0F, .y = 0.0F, .z = 0.0F},
                                                            .rotation = {}});
    const BodyId second = backend.create_body(BodyCreateInfo{.motion_type = BodyMotionType::Dynamic,
                                                             .position = {.x = 2.0F, .y = 0.0F, .z = 0.0F},
                                                             .rotation = {}});

    EXPECT_NE(first, second);

    const std::optional<BodyState> first_state = backend.body_state(first);
    const std::optional<BodyState> second_state = backend.body_state(second);
    ASSERT_TRUE(first_state.has_value());
    ASSERT_TRUE(second_state.has_value());
    EXPECT_FLOAT_EQ(first_state->position.x, 1.0F);
    EXPECT_FLOAT_EQ(second_state->position.x, 2.0F);
}

TEST(NullPhysicsBackend, DestroyingOneBodyDoesNotAffectAnother) {
    NullPhysicsBackend backend;
    const BodyId first = backend.create_body(BodyCreateInfo{.motion_type = BodyMotionType::Static,
                                                            .position = {.x = 1.0F, .y = 0.0F, .z = 0.0F},
                                                            .rotation = {}});
    const BodyId second = backend.create_body(BodyCreateInfo{.motion_type = BodyMotionType::Dynamic,
                                                             .position = {.x = 2.0F, .y = 0.0F, .z = 0.0F},
                                                             .rotation = {}});

    backend.destroy_body(first);

    EXPECT_FALSE(backend.body_state(first).has_value());
    ASSERT_TRUE(backend.body_state(second).has_value());
    EXPECT_FLOAT_EQ(backend.body_state(second)->position.x, 2.0F);
}

TEST(NullPhysicsBackend, QueryingABodyIdThatWasNeverCreatedReturnsNullopt) {
    const NullPhysicsBackend backend;
    const BodyId never_created{.index = 0, .generation = 0};

    EXPECT_FALSE(backend.body_state(never_created).has_value());
}

TEST(NullPhysicsBackend, QueryingTheDefaultNullBodyIdReturnsNullopt) {
    const NullPhysicsBackend backend;

    EXPECT_FALSE(backend.body_state(BodyId{}).has_value());
}

} // namespace
} // namespace atlas::physics
