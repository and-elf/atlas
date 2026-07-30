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
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "cast_time_attack/cast_time_attack.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(CastTimeAttack, BeginCastStartsTheWindUp) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_begin_cast);

    const RequestResult result = dispatcher.dispatch(server.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0.0F,
                                                                                 .max_range = 5.0F,
                                                                                 .damage = 10,
                                                                                 .cast_time_ticks = 30});

    ASSERT_TRUE(result.accepted);
    const cast_time_attack::CastTimeAttack& cast =
        server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get();
    EXPECT_EQ(cast.target, target);
    EXPECT_EQ(cast.min_range, 0.0F);
    EXPECT_EQ(cast.max_range, 5.0F);
    EXPECT_EQ(cast.damage, 10);
    EXPECT_EQ(cast.cast_time_ticks, 30);
    EXPECT_EQ(cast.remaining_ticks, 30);
}

TEST(CastTimeAttack, BeginCastRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef caster = client.host.create_entity();
    const EntityRef target = client.host.create_entity();
    client.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_begin_cast);

    const RequestResult result = dispatcher.dispatch(client.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0.0F,
                                                                                 .max_range = 5.0F,
                                                                                 .damage = 10,
                                                                                 .cast_time_ticks = 30});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(CastTimeAttack, BeginCastRejectedWithoutACastTimeAttackPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity(); // no CastTimeAttack seeded
    const EntityRef target = server.host.create_entity();

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_begin_cast);

    const RequestResult result = dispatcher.dispatch(server.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0.0F,
                                                                                 .max_range = 5.0F,
                                                                                 .damage = 10,
                                                                                 .cast_time_ticks = 30});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "caster has no CastTimeAttack property");
}

TEST(CastTimeAttack, BeginCastRejectedWhileAlreadyCasting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 12});

    request::Dispatcher<cast_time_attack::BeginCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_begin_cast);

    const RequestResult result = dispatcher.dispatch(server.ctx,
                                                     cast_time_attack::BeginCast{.caster = caster,
                                                                                 .target = target,
                                                                                 .obstacle = EntityRef{},
                                                                                 .min_range = 0.0F,
                                                                                 .max_range = 5.0F,
                                                                                 .damage = 20,
                                                                                 .cast_time_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "caster is already casting");
    // Untouched by the rejected request - still the original cast in progress.
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().damage, 10);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 12);
}

TEST(CastTimeAttack, AdvanceCastTicksDownWithoutCompleting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 30});

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 20);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20); // not yet resolved
}

TEST(CastTimeAttack, AdvanceCastLandsWhenCompleteInRangeAndUnobstructed) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10});

    bool landed_published = false;
    std::int32_t published_damage = 0;
    server.ctx.subscribe<cast_time_attack::CastLanded>([&](const cast_time_attack::CastLanded& event) {
        landed_published = event.caster == caster && event.target == target;
        published_damage = event.damage;
    });

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
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
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10});

    server.position_store.set(target, movement::Position{.x = 20.0F, .y = 0.0F});

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    // The wind-up is still consumed - a fizzled cast doesn't linger, waiting
    // for range to become valid again; it must be started over with a fresh
    // BeginCast.
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
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
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = target,
                                                                       .obstacle = obstacle,
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10});

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    EXPECT_EQ(server.ctx.get<cast_time_attack::CastTimeAttack>(caster)->get().remaining_ticks, 0);
}

TEST(CastTimeAttack, AdvanceCastPropagatesHealthsOwnRejectionWithoutHealthOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Health seeded
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = target,
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10});

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Health");
}

TEST(CastTimeAttack, AdvanceCastIsANoOpWhenNotCurrentlyCasting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{}); // remaining_ticks == 0

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_TRUE(result.accepted);
}

TEST(CastTimeAttack, AdvanceCastRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef caster = client.host.create_entity();
    client.cast_time_attack_store.set(caster,
                                      cast_time_attack::CastTimeAttack{.is_casting = true,
                                                                       .target = EntityRef{},
                                                                       .obstacle = EntityRef{},
                                                                       .min_range = 0.0F,
                                                                       .max_range = 5.0F,
                                                                       .damage = 10,
                                                                       .cast_time_ticks = 30,
                                                                       .remaining_ticks = 10});

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(client.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(CastTimeAttack, AdvanceCastRejectedWithoutACastTimeAttackPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity(); // no CastTimeAttack seeded

    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "caster has no CastTimeAttack property");
}

TEST(CastTimeAttack, ZeroCastTimeResolvesOnTheFirstAdvanceCastCall) {
    // An instant ("0 cast time") ability still needs one AdvanceCast call to
    // resolve - BeginCast itself never resolves anything - but that call
    // lands immediately regardless of how small delta_ticks is, since
    // remaining_ticks already started at 0.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});

    request::Dispatcher<cast_time_attack::BeginCast> begin_dispatcher;
    begin_dispatcher.register_handler(cast_time_attack::on_begin_cast);
    ASSERT_TRUE(begin_dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0.0F,
                                                          .max_range = 5.0F,
                                                          .damage = 10,
                                                          .cast_time_ticks = 0})
                    .accepted);

    request::Dispatcher<cast_time_attack::AdvanceCast> advance_dispatcher;
    advance_dispatcher.register_handler(cast_time_attack::on_advance_cast);

    const RequestResult result = advance_dispatcher.dispatch(
        server.ctx, cast_time_attack::AdvanceCast{.caster = caster, .delta_ticks = 0});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
}

} // namespace
} // namespace atlas::demo
