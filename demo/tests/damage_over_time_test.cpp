// Proves damage_over_time: a reusable "recurring damage for a duration"
// mechanism, deliberately generic - it has no notion of fire, poison, or
// any other flavor (spec §2, Mechanism Over Meaning). ApplyDotEffect seeds
// an effect; AdvanceDotEffect - driven explicitly each call, the same
// "caller simulates the tick" pattern auto_attack/aura/cast_time_attack
// already establish - ticks it down and dispatches health::on_apply_damage
// directly once per elapsed interval.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "damage_over_time/damage_over_time.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(DamageOverTime, ApplyDotEffectSeedsTheEffect) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.dot_effect_store.set(target, damage_over_time::DotEffect{});

    request::Dispatcher<damage_over_time::ApplyDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_apply_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        damage_over_time::ApplyDotEffect{
            .target = target, .damage_per_tick = 20, .tick_interval_ticks = 180, .total_applications = 3});

    ASSERT_TRUE(result.accepted);
    const damage_over_time::DotEffect& effect = server.ctx.get<damage_over_time::DotEffect>(target)->get();
    EXPECT_EQ(effect.damage_per_tick, 20);
    EXPECT_EQ(effect.tick_interval_ticks, 180);
    EXPECT_EQ(effect.ticks_until_next, 180);
    EXPECT_EQ(effect.remaining_applications, 3);
}

TEST(DamageOverTime, ApplyDotEffectRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.dot_effect_store.set(target, damage_over_time::DotEffect{});

    request::Dispatcher<damage_over_time::ApplyDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_apply_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        client.ctx,
        damage_over_time::ApplyDotEffect{
            .target = target, .damage_per_tick = 20, .tick_interval_ticks = 180, .total_applications = 3});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(DamageOverTime, ApplyDotEffectRejectedWithoutADotEffectPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no DotEffect seeded

    request::Dispatcher<damage_over_time::ApplyDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_apply_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        damage_over_time::ApplyDotEffect{
            .target = target, .damage_per_tick = 20, .tick_interval_ticks = 180, .total_applications = 3});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no DotEffect property");
}

TEST(DamageOverTime, ApplyDotEffectRefreshesAnAlreadyActiveEffect) {
    // No stacking: a fresh ApplyDotEffect discards whatever was left of a
    // previous one and starts over - recomputed fresh, not accumulated.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 5,
                                                            .tick_interval_ticks = 60,
                                                            .ticks_until_next = 10,
                                                            .remaining_applications = 1});

    request::Dispatcher<damage_over_time::ApplyDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_apply_dot_effect);

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              damage_over_time::ApplyDotEffect{.target = target,
                                                               .damage_per_tick = 20,
                                                               .tick_interval_ticks = 180,
                                                               .total_applications = 3})
                    .accepted);

    const damage_over_time::DotEffect& effect = server.ctx.get<damage_over_time::DotEffect>(target)->get();
    EXPECT_EQ(effect.damage_per_tick, 20);
    EXPECT_EQ(effect.ticks_until_next, 180);
    EXPECT_EQ(effect.remaining_applications, 3);
}

TEST(DamageOverTime, AdvanceDotEffectTicksDownWithoutFiringYet) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 100, .maximum = 100});
    server.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 20,
                                                            .tick_interval_ticks = 180,
                                                            .ticks_until_next = 180,
                                                            .remaining_applications = 3});

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 60});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 100); // not yet
    const damage_over_time::DotEffect& effect = server.ctx.get<damage_over_time::DotEffect>(target)->get();
    EXPECT_EQ(effect.ticks_until_next, 120);
    EXPECT_EQ(effect.remaining_applications, 3);
}

TEST(DamageOverTime, AdvanceDotEffectAppliesDamageAndPublishesDotEffectTickedOnceAnIntervalElapses) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 100, .maximum = 100});
    server.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 20,
                                                            .tick_interval_ticks = 180,
                                                            .ticks_until_next = 180,
                                                            .remaining_applications = 3});

    bool ticked_published = false;
    std::int32_t published_damage = 0;
    server.ctx.subscribe<damage_over_time::DotEffectTicked>(
        [&](const damage_over_time::DotEffectTicked& event) {
            ticked_published = event.target == target;
            published_damage = event.damage;
        });

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 180});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 80);
    EXPECT_TRUE(ticked_published);
    EXPECT_EQ(published_damage, 20);
    const damage_over_time::DotEffect& effect = server.ctx.get<damage_over_time::DotEffect>(target)->get();
    EXPECT_EQ(effect.remaining_applications, 2);
    EXPECT_EQ(effect.ticks_until_next, 180); // reset for the next application
}

TEST(DamageOverTime, TheThirdAndFinalApplicationLeavesTheEffectIdle) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 100, .maximum = 100});
    server.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 20,
                                                            .tick_interval_ticks = 180,
                                                            .ticks_until_next = 180,
                                                            .remaining_applications = 1});

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    ASSERT_TRUE(
        dispatcher
            .dispatch(server.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 180})
            .accepted);

    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 80);
    const damage_over_time::DotEffect& effect = server.ctx.get<damage_over_time::DotEffect>(target)->get();
    EXPECT_EQ(effect.remaining_applications, 0);
    EXPECT_EQ(effect.ticks_until_next, 0);
}

TEST(DamageOverTime, AdvanceDotEffectIsANoOpOnceExhausted) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 100, .maximum = 100});
    server.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 20,
                                                            .tick_interval_ticks = 180,
                                                            .ticks_until_next = 0,
                                                            .remaining_applications = 0});

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 999});

    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 100);
}

TEST(DamageOverTime, AdvanceDotEffectRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 20,
                                                            .tick_interval_ticks = 180,
                                                            .ticks_until_next = 180,
                                                            .remaining_applications = 3});

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        client.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 180});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(DamageOverTime, AdvanceDotEffectRejectedWithoutADotEffectPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no DotEffect seeded

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 180});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no DotEffect property");
}

TEST(DamageOverTime, AdvanceDotEffectPropagatesHealthsOwnRejectionWithoutHealthOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no Health seeded
    server.dot_effect_store.set(target,
                                damage_over_time::DotEffect{.damage_per_tick = 20,
                                                            .tick_interval_ticks = 180,
                                                            .ticks_until_next = 180,
                                                            .remaining_applications = 3});

    request::Dispatcher<damage_over_time::AdvanceDotEffect> dispatcher;
    dispatcher.register_handler(damage_over_time::on_advance_dot_effect);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 180});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Health");
}

TEST(DamageOverTime, ThreeApplicationsOverNineSecondsDealTheFullTotal) {
    // The concrete Fireball-shaped scenario this mechanism exists for:
    // 20% of some original hit, three times, three seconds apart (spec
    // §core::Time::ticks_per_second == 60, so 3 seconds == 180 ticks).
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 100, .maximum = 100});
    server.dot_effect_store.set(target, damage_over_time::DotEffect{});

    request::Dispatcher<damage_over_time::ApplyDotEffect> apply_dispatcher;
    apply_dispatcher.register_handler(damage_over_time::on_apply_dot_effect);
    ASSERT_TRUE(apply_dispatcher
                    .dispatch(server.ctx,
                              damage_over_time::ApplyDotEffect{.target = target,
                                                               .damage_per_tick = 20,
                                                               .tick_interval_ticks = 180,
                                                               .total_applications = 3})
                    .accepted);

    request::Dispatcher<damage_over_time::AdvanceDotEffect> advance_dispatcher;
    advance_dispatcher.register_handler(damage_over_time::on_advance_dot_effect);
    for (int application = 0; application < 3; ++application) {
        ASSERT_TRUE(advance_dispatcher
                        .dispatch(server.ctx,
                                  damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = 180})
                        .accepted);
    }

    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 40); // 100 - 3*20
    EXPECT_EQ(server.ctx.get<damage_over_time::DotEffect>(target)->get().remaining_applications, 0);
}

} // namespace
} // namespace atlas::demo
