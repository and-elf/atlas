// Issue #188: proves atlas-physics' PhysicsBackend feeding a real,
// depends_on/consumes-ordered capability dependency graph - spec §5's own
// illustrative Physics["physics"] --> Entity diagram, made real (see
// demo/modules/rigid_body/rigid_body.hpp's own top-of-file comment for the
// full mechanism, and demo/README.md's "Physics" section).
//
// Composes a deliberately minimal test host (CLAUDE.md: "test a capability
// by composing it into a minimal test host, not by mocking its behavior") -
// PhysicsTestHost, below, wraps the generated PhysicsCompositionHost
// (physics_host.host.yaml: movement + rigid_body + physics_observer only),
// mirroring demo/tests/simulated_host.hpp's own SimulatedHost shape at a
// smaller scale rather than reusing SimulatedHost itself, which composes
// every other demo capability this scenario doesn't need.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <gtest/gtest.h>

#include "movement/movement.hpp"
#include "physics_host.host.hpp"
#include "physics_observer/physics_observer.hpp"
#include "rigid_body/rigid_body.hpp"

namespace atlas::demo {
namespace {

stage::StageSequence make_physics_sequence() {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return std::move(*sequence);
}

// See this file's own top-of-file comment for why this is a second, smaller
// host shape rather than a reuse of demo/tests/simulated_host.hpp's
// SimulatedHost.
struct PhysicsTestHost {
    explicit PhysicsTestHost(bool has_authority) : host(make_physics_sequence(), has_authority), ctx(host) {
        register_property_stores(ctx, composition);
    }

    runtime::Host host;
    Context ctx;
    PhysicsCompositionHost composition;

    // This capability's own backend + private per-entity registry (see
    // rigid_body.hpp) - owned here, by whoever composes rigid_body into this
    // host, exactly like movement::ContributionRegistry/
    // auto_attack::ActionRegistry are owned by SimulatedHost rather than
    // living as namespace-scope globals.
    rigid_body::Backend backend;
    rigid_body::BodyRegistry bodies;
};

// A tick's own reading of both sides of the ordering proof: the fresh
// ground-truth height (rigid_body's own BodyState.position_y for *this*
// tick) and whether physics_observer's own ObservedBodyState.height
// diverged from it - the signature of a wrong job registration order (see
// DownstreamConsumerObservesEachTicksFreshlySteppedBodyStateNeverAStaleOne's
// own doc comment). Factored out purely to keep each TEST's own cognitive
// complexity down - GoogleTest's ASSERT_*/EXPECT_* macro expansion inflates
// readability-function-cognitive-complexity's count for a loop body with
// several assertions in it, the same real (not hypothetical) finding
// tests/atlas-physics/jolt_physics_backend_test.cpp's own bit-exact
// comparison helpers were factored out to fix (see atlas-physics' own
// README, "Verification (issue #179)").
struct TickObservation {
    float height = 0.0F;
    bool stale = false;
};

TickObservation observe_tick(PhysicsTestHost& server, EntityRef body, float previous_height) {
    const auto body_state = server.ctx.get<rigid_body::BodyState>(body);
    const auto observed = server.ctx.get<physics_observer::ObservedBodyState>(body);
    EXPECT_TRUE(body_state.has_value());
    EXPECT_TRUE(observed.has_value());
    if (!body_state.has_value() || !observed.has_value()) {
        return TickObservation{.height = previous_height, .stale = false};
    }

    const float current_height = body_state->get().position_y;
    const float observed_height = observed->get().height;
    const bool stale = observed_height != current_height;
    if (stale) {
        // A wrong job registration order (consumer scheduled before
        // physics) makes the consumer read the *previous* tick's
        // BodyState - exactly previous_height, not merely "some other
        // value" - see
        // ReversedJobRegistrationOrderObservesAStaleHeightOneTickBehind's
        // own doc comment.
        EXPECT_FLOAT_EQ(observed_height, previous_height);
    }
    return TickObservation{.height = current_height, .stale = stale};
}

RequestResult spawn(PhysicsTestHost& server, EntityRef entity, bool is_dynamic, float spawn_height) {
    request::Dispatcher<rigid_body::SpawnRigidBody> dispatcher;
    dispatcher.register_handler([&server](Context& ctx, const rigid_body::SpawnRigidBody& cmd) {
        return rigid_body::on_spawn_rigid_body(ctx, server.backend, server.bodies, cmd);
    });
    return dispatcher.dispatch(
        server.ctx,
        rigid_body::SpawnRigidBody{.entity = entity, .is_dynamic = is_dynamic, .spawn_height = spawn_height});
}

// Spawns a Dynamic body at (0, 0) with the given spawn_height, then
// registers both capabilities' per-tick jobs against stage - in dependency
// order (rigid_body's step job first) when physics_scheduled_first is true,
// or deliberately reversed (the bug this issue's test methodology exists to
// catch) when false. Shared by the three DAG-ordering tests below purely to
// keep each one's own cognitive complexity down (see observe_tick's own
// comment for why that matters here specifically).
void spawn_and_schedule(PhysicsTestHost& server,
                        EntityRef body,
                        float spawn_height,
                        const stage::StageId& stage,
                        bool physics_scheduled_first) {
    server.ctx.set<movement::Position>(body, movement::Position{.x = 0.0F, .y = 0.0F});
    ASSERT_TRUE(spawn(server, body, /*is_dynamic=*/true, spawn_height).accepted);

    const auto schedule_physics = [&server, &stage] {
        ASSERT_TRUE(rigid_body::schedule_step_job(server.host,
                                                  stage,
                                                  server.ctx,
                                                  server.backend,
                                                  server.bodies,
                                                  rigid_body::k_fixed_timestep_seconds));
    };
    const auto schedule_observer = [&server, &stage, body] {
        ASSERT_TRUE(physics_observer::schedule_observation_job(server.host, stage, server.ctx, {body}));
    };

    if (physics_scheduled_first) {
        schedule_physics();
        schedule_observer();
    } else {
        schedule_observer();
        schedule_physics();
    }
}

TEST(RigidBody, SpawnRigidBodyRejectedWithoutAuthority) {
    PhysicsTestHost client{/*has_authority=*/false};
    const EntityRef entity = client.host.create_entity();
    client.ctx.set<movement::Position>(entity, movement::Position{.x = 0.0F, .y = 0.0F});

    const RequestResult result = spawn(client, entity, /*is_dynamic=*/true, /*spawn_height=*/1.0F);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(RigidBody, SpawnRigidBodyRejectedWithoutAPositionSeeded) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef entity = server.host.create_entity(); // no Position seeded

    const RequestResult result = spawn(server, entity, /*is_dynamic=*/true, /*spawn_height=*/1.0F);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "entity has no Position");
}

TEST(RigidBody, SpawnRigidBodyRejectedWhenEntityAlreadyHasABody) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef entity = server.host.create_entity();
    server.ctx.set<movement::Position>(entity, movement::Position{.x = 1.0F, .y = 2.0F});

    ASSERT_TRUE(spawn(server, entity, /*is_dynamic=*/true, /*spawn_height=*/5.0F).accepted);

    const RequestResult second = spawn(server, entity, /*is_dynamic=*/true, /*spawn_height=*/5.0F);
    EXPECT_FALSE(second.accepted);
    EXPECT_EQ(second.rejection_reason, "entity already has a rigid body");
}

TEST(RigidBody, SpawnRigidBodySeedsBodyStateFromPositionAndSpawnHeight) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef entity = server.host.create_entity();
    server.ctx.set<movement::Position>(entity, movement::Position{.x = 3.0F, .y = -4.0F});

    ASSERT_TRUE(spawn(server, entity, /*is_dynamic=*/false, /*spawn_height=*/7.0F).accepted);

    const auto body_state = server.ctx.get<rigid_body::BodyState>(entity);
    ASSERT_TRUE(body_state.has_value());
    EXPECT_FLOAT_EQ(body_state->get().position_x, 3.0F);
    EXPECT_FLOAT_EQ(body_state->get().position_y, 7.0F);
    EXPECT_FLOAT_EQ(body_state->get().position_z, -4.0F);
}

TEST(RigidBody, AdvanceRigidBodiesLeavesBodyStateUnchangedWhenTheBackendBodyNoLongerExists) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef entity = server.host.create_entity();
    server.ctx.set<movement::Position>(entity, movement::Position{.x = 0.0F, .y = 0.0F});
    ASSERT_TRUE(spawn(server, entity, /*is_dynamic=*/true, /*spawn_height=*/1.0F).accepted);

    ASSERT_EQ(server.bodies.size(), 1U);
    server.backend.destroy_body(server.bodies.front().second);

    const float before = server.ctx.get<rigid_body::BodyState>(entity)->get().position_y;
    rigid_body::advance_rigid_bodies(
        server.ctx, server.backend, server.bodies, rigid_body::k_fixed_timestep_seconds);
    const float after = server.ctx.get<rigid_body::BodyState>(entity)->get().position_y;

    EXPECT_FLOAT_EQ(before, after);
}

TEST(RigidBody, ScheduleStepJobReturnsFalseForAStageNotInTheHostsSequence) {
    PhysicsTestHost server{/*has_authority=*/true};
    const stage::StageId unknown_stage{"DoesNotExist"};

    EXPECT_FALSE(rigid_body::schedule_step_job(server.host,
                                               unknown_stage,
                                               server.ctx,
                                               server.backend,
                                               server.bodies,
                                               rigid_body::k_fixed_timestep_seconds));
}

TEST(PhysicsObserver, ObserveBodyHeightsSkipsEntitiesWithoutABodyState) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef entity = server.host.create_entity(); // never spawned

    physics_observer::observe_body_heights(server.ctx, {entity});

    EXPECT_FALSE(server.ctx.get<physics_observer::ObservedBodyState>(entity).has_value());
}

TEST(PhysicsObserver, ScheduleObservationJobReturnsFalseForAStageNotInTheHostsSequence) {
    PhysicsTestHost server{/*has_authority=*/true};
    const stage::StageId unknown_stage{"DoesNotExist"};

    EXPECT_FALSE(physics_observer::schedule_observation_job(server.host, unknown_stage, server.ctx, {}));
}

// The real DAG-integration proof (issue #188's own required test): a Dynamic
// body composed via rigid_body, its per-tick physics step registered as a
// real Host::schedule() Job (never a hand-called function from the test
// loop itself - see rigid_body.hpp's own top-of-file comment for why this
// is the genuine "runs every tick regardless" mechanism), and
// physics_observer's own downstream job - consuming rigid_body's BodyState
// via consumes: [BodyState], never a depends_on: [rigid_body] entry -
// registered against the *same* stage, in dependency order (rigid_body's
// job first, since physics_observer::consumes: [BodyState] resolves to a
// "physics_observer depends on rigid_body" edge, spec §5 Property-Level
// Ordering). atlas::advance_tick (the real per-tick entry point
// demo/host_loop.cpp's own run_ticks already exercises) is what actually
// invokes both jobs, once per tick, in that fixed registration order -
// nothing in this test calls either capability's own per-tick function
// directly.
//
// Every tick, this asserts physics_observer's own ObservedBodyState.height
// exactly equals rigid_body's own BodyState.position_y for *that same
// tick* - the invariant a wrong registration order (consumer scheduled
// before physics) would break, since the consumer would then read the
// *previous* tick's BodyState instead. See
// ReversedJobRegistrationOrderObservesAStaleHeightOneTickBehind below for
// what that actual failure mode looks like, proving this isn't a tautology.
TEST(RigidBody, DownstreamConsumerObservesEachTicksFreshlySteppedBodyStateNeverAStaleOne) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef body = server.host.create_entity();
    const stage::StageId simulation_stage{"Simulation"};
    spawn_and_schedule(
        server, body, /*spawn_height=*/10.0F, simulation_stage, /*physics_scheduled_first=*/true);

    float height = 10.0F;
    for (int tick = 0; tick < 10; ++tick) {
        advance_tick(server.host, server.ctx);

        const TickObservation observation = observe_tick(server, body, height);
        EXPECT_FALSE(observation.stale);
        height = observation.height;
    }
}

#if defined(ATLAS_DEMO_PHYSICS_BACKEND_JOLT)

// Only meaningful against the real JoltPhysicsBackend: NullPhysicsBackend's
// step() is a genuine no-op (its own doc comment), so a body's height never
// changes tick to tick under it - there would be nothing for a wrong
// registration order to make stale. Mirrors
// JoltPhysicsBackend.DynamicBodyFallsUnderGravityWithinPlausibleRange's own
// plausible-band methodology (tests/atlas-physics/jolt_physics_backend_test.cpp).
TEST(RigidBody, DynamicBodyFallsUnderRealGravityWhileStayingInSyncWithTheDownstreamConsumer) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef body = server.host.create_entity();
    const stage::StageId simulation_stage{"Simulation"};
    spawn_and_schedule(
        server, body, /*spawn_height=*/10.0F, simulation_stage, /*physics_scheduled_first=*/true);

    float last_height = 10.0F;
    for (int tick = 0; tick < 60; ++tick) {
        advance_tick(server.host, server.ctx);

        const TickObservation observation = observe_tick(server, body, last_height);
        EXPECT_FALSE(observation.stale);
        last_height = observation.height;
    }

    // 1 simulated second (60 ticks at 1/60s) under standard gravity falls
    // close to 4.905m; a wide +-50% band avoids coupling this test to
    // Jolt's own exact integration constants (the same band
    // JoltPhysicsBackend's own gravity test uses).
    const float total_fall = 10.0F - last_height;
    EXPECT_GT(total_fall, 2.4525F);
    EXPECT_LT(total_fall, 7.3575F);
}

// Deliberately registers physics_observer's job BEFORE rigid_body's own step
// job - the exact bug this issue's test methodology exists to catch.
// Demonstrates concretely what "the consumer ran before physics stepped"
// looks like: the observed height this tick equals the *previous* tick's
// ground truth, not the current one - proving
// DownstreamConsumerObservesEachTicksFreshlySteppedBodyStateNeverAStaleOne's
// own same-tick-equality assertion is a real, meaningful invariant, not one
// that would have passed regardless of registration order.
TEST(RigidBody, ReversedJobRegistrationOrderObservesAStaleHeightOneTickBehind) {
    PhysicsTestHost server{/*has_authority=*/true};
    const EntityRef body = server.host.create_entity();
    const stage::StageId simulation_stage{"Simulation"};
    // Reversed order, relative to the two tests above: the consumer is
    // scheduled first.
    spawn_and_schedule(
        server, body, /*spawn_height=*/10.0F, simulation_stage, /*physics_scheduled_first=*/false);

    float previous_height = 10.0F;
    bool observed_a_stale_value = false;
    for (int tick = 0; tick < 60; ++tick) {
        advance_tick(server.host, server.ctx);

        const TickObservation observation = observe_tick(server, body, previous_height);
        observed_a_stale_value = observed_a_stale_value || observation.stale;
        previous_height = observation.height;
    }

    EXPECT_TRUE(observed_a_stale_value);
}

#endif // ATLAS_DEMO_PHYSICS_BACKEND_JOLT

} // namespace
} // namespace atlas::demo
