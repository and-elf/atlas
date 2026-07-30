// Proves cast_time_attack: a targeted attack resolved only after a wind-up
// (BeginCast starts it, AdvanceCast - driven explicitly each call, the same
// "caller simulates the tick" pattern auto_attack_test.cpp/aura_test.cpp
// already establish - ticks it down and, once complete, resolves it via
// attack_resolution::resolve_targeted_attack, exactly like
// auto_attack::on_try_auto_attack does once off cooldown). Range and line of
// sight are deliberately not checked at BeginCast time at all - only once
// the cast completes - so every "fizzle" case here seeds the target moved
// out of range or behind an obstacle only after casting has already begun,
// proving the check really does happen at completion, not at the start.
//
// The cast's own lifecycle (atlas::runtime::ActionState, in
// server.cast_action_registry - see atlas-runtime's own README section)
// replaced a plain is_casting bool: cancellation (movement, or
// interruption::ActionInterrupted) only *queues* a pending cancel
// (request_cancel) - the actual transition to Cancelled happens the next
// time AdvanceCast runs, via atlas::runtime::advance_action, which checks
// for it *before* any of AdvanceCast's own per-tick logic. That two-step
// shape (queue, then apply on the next advance) is deliberate - it is what
// "the runtime handles cancel first" means as literal control flow rather
// than an out-of-band mutation the instant a cancelling event arrives - so
// the cancellation tests below are each split into "queues it" and "applies
// it on the next AdvanceCast" rather than asserting an immediate effect.
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/action.hpp"

#include <cstdint>
#include <gtest/gtest.h>

#include "cast_time_attack/cast_time_attack.hpp"
#include "haste/haste.hpp"
#include "interruption/interruption.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

// Shared dispatcher-construction helpers - every test needs to bind
// on_begin_cast/on_advance_cast against a particular host's
// ActionRegistry, and inlining that registration lambda in each test body
// was pushing several tests' own cognitive complexity over clang-tidy's
// threshold. Capturing registry (a reference parameter) by reference here
// binds directly to the caller's actual registry object - references have
// no separate storage of their own to dangle - so the returned Dispatcher
// stays valid for the rest of the calling test.
request::Dispatcher<cast_time_attack::BeginCast>
make_begin_cast_dispatcher(cast_time_attack::ActionRegistry& registry) {
    request::Dispatcher<cast_time_attack::BeginCast> dispatcher;
    dispatcher.register_handler([&registry](Context& ctx, const cast_time_attack::BeginCast& cmd) {
        return cast_time_attack::on_begin_cast(ctx, registry, cmd);
    });
    return dispatcher;
}

request::Dispatcher<cast_time_attack::AdvanceCast>
make_advance_cast_dispatcher(cast_time_attack::ActionRegistry& registry) {
    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler([&registry](Context& ctx, const cast_time_attack::AdvanceCast& cmd) {
        return cast_time_attack::on_advance_cast(ctx, registry, cmd);
    });
    return dispatcher;
}

TEST(CastTimeAttack, BeginCastStartsTheWindUp) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    const RequestResult result = dispatcher.dispatch(server.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0,
                                                                                 .max_range = 5,
                                                                                 .damage = 10,
                                                                                 .cast_time_ticks = 30,
                                                                                 .requires_stationary = false,
                                                                                 .animation = ResourceId{}});

    ASSERT_TRUE(result.accepted);
    const cast_time_attack::CastTimeAttack& cast =
        server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get();
    EXPECT_EQ(cast.target, target);
    EXPECT_EQ(cast.min_range, 0);
    EXPECT_EQ(cast.max_range, 5);
    EXPECT_EQ(cast.damage, 10);
    EXPECT_EQ(cast.cast_time_ticks, 30);
    EXPECT_EQ(cast.remaining_ticks, 30);

    const cast_time_attack::CastAction& action = server.cast_action_registry.at(caster);
    EXPECT_EQ(action.action_state, runtime::ActionState::Started);
    EXPECT_FALSE(action.cancel_requested);
}

TEST(CastTimeAttack, BeginCastRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef caster = client.host.create_entity();
    const EntityRef target = client.host.create_entity();
    client.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(client.cast_action_registry);

    const RequestResult result = dispatcher.dispatch(client.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0,
                                                                                 .max_range = 5,
                                                                                 .damage = 10,
                                                                                 .cast_time_ticks = 30,
                                                                                 .requires_stationary = false,
                                                                                 .animation = ResourceId{}});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(CastTimeAttack, BeginCastRejectedWithoutACastTimeAttackPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity(); // no CastTimeAttack seeded
    const EntityRef target = server.host.create_entity();

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    const RequestResult result = dispatcher.dispatch(server.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0,
                                                                                 .max_range = 5,
                                                                                 .damage = 10,
                                                                                 .cast_time_ticks = 30,
                                                                                 .requires_stationary = false,
                                                                                 .animation = ResourceId{}});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "caster has no CastTimeAttack property");
}

TEST(CastTimeAttack, BeginCastRejectedWhileAlreadyCasting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Started, .cancel_requested = false};

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    const RequestResult result = dispatcher.dispatch(server.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0,
                                                                                 .max_range = 5,
                                                                                 .damage = 20,
                                                                                 .cast_time_ticks = 10,
                                                                                 .requires_stationary = false,
                                                                                 .animation = ResourceId{}});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "caster is already casting");
    // Untouched by the rejected request - still the original cast in progress.
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().damage, 10);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 12);
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Started);
}

TEST(CastTimeAttack, AdvanceCastTicksDownWithoutCompleting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 30,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Started, .cancel_requested = false};

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 20);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20); // not yet resolved
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Ongoing);
}

TEST(CastTimeAttack, AdvanceCastLandsWhenCompleteInRangeAndUnobstructed) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    bool landed_published = false;
    std::int32_t published_damage = 0;
    server.ctx.subscribe<cast_time_attack::CastLanded>([&](const cast_time_attack::CastLanded& event) {
        landed_published = event.caster == caster && event.target == target;
        published_damage = event.damage;
    });

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Completed);
    EXPECT_TRUE(landed_published);
    EXPECT_EQ(published_damage, 10);
}

TEST(CastTimeAttack, AdvanceCastFizzlesWhenTargetMovedOutOfRangeBeforeCompletion) {
    // Range is never checked at BeginCast time - only here, at completion -
    // proven by seeding the target within range, then moving it out before
    // the cast finishes.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    server.position_store.set(target, movement::Position{.x = 20.0F, .y = 0.0F});

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    // The wind-up is still consumed - a fizzled cast doesn't linger, waiting
    // for range to become valid again; it must be started over with a fresh
    // BeginCast.
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Completed);
}

TEST(CastTimeAttack, AdvanceCastFizzlesWhenLineOfSightIsBlockedAtCompletion) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 1.5F, .center_y = 0.0F, .radius = 1.0F});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = obstacle,
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Completed);
}

TEST(CastTimeAttack, AdvanceCastPropagatesHealthsOwnRejectionWithoutHealthOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Health seeded
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Health");
    // The cast still completed its wind-up (whether or not resolution
    // itself accepted) - the same "landing propagates health's own
    // rejection, checked after the state transition already happened"
    // shape auto_attack established.
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Completed);
}

TEST(CastTimeAttack, AdvanceCastIsANoOpWhenNotCurrentlyCasting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});
    // No registry entry seeded - defaults to CastAction{} (Completed), the
    // same idle bucket "never cast anything" and "a previous cast already
    // resolved" both fall into.

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_TRUE(result.accepted);
}

TEST(CastTimeAttack, AdvanceCastRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef caster = client.host.create_entity();
    client.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = EntityRef{},
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10,
                                                                       .animation = ResourceId{}});
    client.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(client.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(client.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(CastTimeAttack, AdvanceCastRejectedWithoutACastTimeAttackPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity(); // no CastTimeAttack seeded

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "caster has no CastTimeAttack property");
}

TEST(CastTimeAttack, ZeroCastTimeResolvesOnTheFirstAdvanceCastCall) {
    // An instant ("0 cast time") ability still needs one AdvanceCast call to
    // resolve - BeginCast itself never resolves anything - but that call
    // lands immediately regardless of how small delta_ticks is, since
    // remaining_ticks already started at 0. Started (set by BeginCast) is
    // not a terminal ActionState, so advance_action still runs on_advance
    // this call rather than treating it as already-idle.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    request::Dispatcher<cast_time_attack::BeginCast> begin_dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);
    ASSERT_TRUE(begin_dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0,
                                                          .max_range = 5,
                                                          .damage = 10,
                                                          .cast_time_ticks = 0,
                                                          .requires_stationary = false,
                                                          .animation = ResourceId{}})
                    .accepted);

    request::Dispatcher<cast_time_attack::AdvanceCast> advance_dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result = advance_dispatcher.dispatch(
        server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 0});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Completed);
}

TEST(CastTimeAttack, MovementQueuesCancellationOfARequiresStationaryCast) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    cast_time_attack::on_movement_occurred(
        server.ctx,
        server.cast_action_registry,
        movement::PositionChanged{.target = caster, .new_x = 1.0F, .new_y = 0.0F});

    // Queued, not yet applied: action_state is unchanged this call - only
    // the next AdvanceCast actually transitions it (see the test below).
    const cast_time_attack::CastAction& action = server.cast_action_registry.at(caster);
    EXPECT_EQ(action.action_state, runtime::ActionState::Ongoing);
    EXPECT_TRUE(action.cancel_requested);
}

TEST(CastTimeAttack, MovementDoesNotQueueCancellationOfACastThatDoesNotRequireStandingStill) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    cast_time_attack::on_movement_occurred(
        server.ctx,
        server.cast_action_registry,
        movement::PositionChanged{.target = caster, .new_x = 1.0F, .new_y = 0.0F});

    EXPECT_FALSE(server.cast_action_registry.at(caster).cancel_requested);
}

TEST(CastTimeAttack, MovementOfAnUnrelatedEntityIsIgnored) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef bystander = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    cast_time_attack::on_movement_occurred(
        server.ctx,
        server.cast_action_registry,
        movement::PositionChanged{.target = bystander, .new_x = 1.0F, .new_y = 0.0F});

    EXPECT_FALSE(server.cast_action_registry.at(caster).cancel_requested);
}

TEST(CastTimeAttack, ActionInterruptedQueuesCancellationRegardlessOfRequiresStationary) {
    // The generic mechanism: unlike movement, this ignores requires_stationary
    // entirely - a crowd-control effect (stun, disorient - not built in this
    // demo, see this capability's README section) should interrupt any cast,
    // not just ones that opted into caring about movement.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    cast_time_attack::on_action_interrupted(server.cast_action_registry,
                                            interruption::ActionInterrupted{.entity = caster});

    EXPECT_TRUE(server.cast_action_registry.at(caster).cancel_requested);
}

TEST(CastTimeAttack, ActionInterruptedOfAnUnrelatedEntityIsIgnored) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef bystander = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = false,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    cast_time_attack::on_action_interrupted(server.cast_action_registry,
                                            interruption::ActionInterrupted{.entity = bystander});

    EXPECT_FALSE(server.cast_action_registry.at(caster).cancel_requested);
}

TEST(CastTimeAttack, CancellationIsANoOpForAnEntityWithNoRegistryEntry) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef bystander = server.host.create_entity(); // never cast anything

    // Neither call should throw or crash - an event about an entity this
    // capability has no in-progress state for is simply irrelevant, not an
    // error.
    cast_time_attack::on_movement_occurred(
        server.ctx,
        server.cast_action_registry,
        movement::PositionChanged{.target = bystander, .new_x = 1.0F, .new_y = 0.0F});
    cast_time_attack::on_action_interrupted(server.cast_action_registry,
                                            interruption::ActionInterrupted{.entity = bystander});

    EXPECT_EQ(server.cast_action_registry.find(bystander), server.cast_action_registry.end());
}

TEST(CastTimeAttack, AdvanceCastAppliesAQueuedCancellationBeforeAnyNormalTicking) {
    // The other half of the two-step story above: once cancel_requested is
    // set (however it got set), the *next* AdvanceCast call is where
    // cancellation is actually applied - via advance_action, checked before
    // this function's own per-tick logic ever runs.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] =
        cast_time_attack::CastAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = true};

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    // Cancelled outright - not ticked down by delta_ticks at all, and
    // nothing was ever attempted against target.
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
    const cast_time_attack::CastAction& action = server.cast_action_registry.at(caster);
    EXPECT_EQ(action.action_state, runtime::ActionState::Cancelled);
    EXPECT_FALSE(action.cancel_requested);
}

TEST(CastTimeAttack, DispatchingMoveThenAdvanceCastCancelsARequiresStationaryCastEndToEnd) {
    // Proves SimulatedHost's own subscription wiring (demo/tests/simulated_host.hpp)
    // actually connects a real movement::Move dispatch through to
    // cast_time_attack's queued-cancellation flag, and that the following
    // AdvanceCast call is where that queued cancellation actually applies.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(caster, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, caster, 10.0F);
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.requires_stationary = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0,
                                                                       .max_range = 5,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12,
                                                                       .animation = ResourceId{}});
    server.cast_action_registry[caster] = cast_time_attack::CastAction{
        .action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    request::Dispatcher<movement::Move> move_dispatcher;
    move_dispatcher.register_handler(movement::on_move);
    ASSERT_TRUE(
        move_dispatcher
            .dispatch(
                server.ctx,
                movement::Move{.target = caster, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60})
            .accepted);

    EXPECT_TRUE(server.cast_action_registry.at(caster).cancel_requested);
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Ongoing);

    request::Dispatcher<cast_time_attack::AdvanceCast> advance_dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);
    ASSERT_TRUE(advance_dispatcher
                    .dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 5})
                    .accepted);

    const cast_time_attack::CastAction& action = server.cast_action_registry.at(caster);
    EXPECT_EQ(action.action_state, runtime::ActionState::Cancelled);
    EXPECT_FALSE(action.cancel_requested);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
}

TEST(CastTimeAttack, BeginCastPublishesCastStartedWithTheAuthoredDurationWhenNoHasteIsActive) {
    // No haste::HasteSource/CastSpeed setup at all here - an entity with no
    // CastSpeed contribution resolves to no speedup, exactly like an entity
    // with no Armor resolves to no mitigation, so the published duration
    // matches cast_time_ticks exactly.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const ResourceId cast_animation = ResourceId::from_name("animations/fireball_cast");
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    bool started_published = false;
    ResourceId published_animation;
    std::uint64_t published_duration_ticks = 0;
    server.ctx.subscribe<cast_time_attack::CastStarted>([&](const cast_time_attack::CastStarted& event) {
        started_published = event.caster == caster;
        published_animation = event.animation;
        published_duration_ticks = event.duration_ticks;
    });

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0,
                                                          .max_range = 5,
                                                          .damage = 10,
                                                          .cast_time_ticks = 30,
                                                          .requires_stationary = false,
                                                          .animation = cast_animation})
                    .accepted);

    EXPECT_TRUE(started_published);
    EXPECT_EQ(published_animation, cast_animation);
    EXPECT_EQ(published_duration_ticks, 30);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().animation, cast_animation);
}

TEST(CastTimeAttack, BeginCastLocksInAShorterDurationWhenCastSpeedIsHasted) {
    // Simulates an already-resolved haste effect directly (haste_test.cpp's
    // job is proving haste::on_refresh_haste_effect itself resolves
    // CastSpeed correctly) - this test's only concern is that BeginCast
    // actually consumes whatever effective CastSpeed it finds.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});
    server.cast_speed_store.set(caster, haste::CastSpeed{.base = 2.0F});

    std::uint64_t published_duration_ticks = 0;
    server.ctx.subscribe<cast_time_attack::CastStarted>(
        [&](const cast_time_attack::CastStarted& event) { published_duration_ticks = event.duration_ticks; });

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0,
                                                          .max_range = 5,
                                                          .damage = 10,
                                                          .cast_time_ticks = 10,
                                                          .requires_stationary = false,
                                                          .animation = ResourceId{}})
                    .accepted);

    // 10 ticks at 2x CastSpeed: half the authored duration.
    EXPECT_EQ(published_duration_ticks, 5);
    const cast_time_attack::CastTimeAttack& cast =
        server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get();
    EXPECT_EQ(cast.cast_time_ticks, 5);
    EXPECT_EQ(cast.remaining_ticks, 5);
}

TEST(CastTimeAttack, HastedCastStillCompletesAfterItsShortenedDuration) {
    // The concrete demo this mechanism exists for: a hasted cast's
    // simulated animation is shorter than the authored cast_time_ticks, but
    // still runs to completion - AdvanceCast is driven for exactly the
    // shortened (hasted) duration, not the original one, and the cast
    // lands.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});
    server.cast_speed_store.set(caster, haste::CastSpeed{.base = 2.0F});

    request::Dispatcher<cast_time_attack::BeginCast> begin_dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);
    ASSERT_TRUE(begin_dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0,
                                                          .max_range = 5,
                                                          .damage = 10,
                                                          .cast_time_ticks = 10,
                                                          .requires_stationary = false,
                                                          .animation = ResourceId{}})
                    .accepted);

    const std::uint64_t hasted_duration_ticks =
        server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks;
    ASSERT_EQ(hasted_duration_ticks, 5); // shorter than the authored 10

    request::Dispatcher<cast_time_attack::AdvanceCast> advance_dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);
    const RequestResult result = advance_dispatcher.dispatch(
        server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = hasted_duration_ticks});

    ASSERT_TRUE(result.accepted);
    // Complete, not fizzled or still pending: the shortened cast ran to its
    // own full (hasted) duration and landed.
    EXPECT_EQ(server.cast_action_registry.at(caster).action_state, runtime::ActionState::Completed);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
}

TEST(CastTimeAttack, BeginCastTreatsANonPositiveCastSpeedMultiplierAsNoHaste) {
    // A CastSpeed base of 0 (or negative) is nonsensical authored content,
    // not a real haste value - guarded against here rather than dividing by
    // it, which would otherwise convert an infinite/NaN double into
    // std::uint64_t (undefined behavior).
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});
    server.cast_speed_store.set(caster, haste::CastSpeed{.base = 0.0F});

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0,
                                                          .max_range = 5,
                                                          .damage = 10,
                                                          .cast_time_ticks = 10,
                                                          .requires_stationary = false,
                                                          .animation = ResourceId{}})
                    .accepted);

    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 10);
}

} // namespace
} // namespace atlas::demo
