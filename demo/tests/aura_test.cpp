// Proves aura's range-based effect on movement::MovementSpeed: ActivateAura
// seeds a source's declared range/multiplier, and RefreshAuraEffect - the
// per-tick re-evaluation a WhileCondition contribution needs (spec §20; see
// movement::refresh_speed_with_transient_contributions and this module's
// README section for why it can never be a stored, incrementally
// added/removed entry) - applies the multiplier only while the target is
// within range, recomputed fresh on every call rather than persisted. A
// real worked scenario (target moves from inside a haste zone's range to
// outside it, calling RefreshAuraEffect before and after) proves the effect
// actually stops, not just that it starts.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "aura/aura.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Aura, ActivateAuraSeedsRangeAndMultiplier) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    server.aura_source_store.set(source, aura::AuraSource{.range = 0.0F, .multiplier = 1.0F});

    request::Dispatcher<aura::ActivateAura> dispatcher;
    dispatcher.register_handler(aura::on_activate_aura);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, aura::ActivateAura{.source = source, .range = 5.0F, .multiplier = 1.2F});

    ASSERT_TRUE(result.accepted);
    const auto aura_source = server.ctx.get<aura::AuraSource>(source);
    ASSERT_TRUE(aura_source.has_value());
    EXPECT_FLOAT_EQ(aura_source->get().range, 5.0F);
    EXPECT_FLOAT_EQ(aura_source->get().multiplier, 1.2F);
}

TEST(Aura, ActivateAuraRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef source = client.host.create_entity();
    client.aura_source_store.set(source, aura::AuraSource{.range = 0.0F, .multiplier = 1.0F});

    request::Dispatcher<aura::ActivateAura> dispatcher;
    dispatcher.register_handler(aura::on_activate_aura);

    const RequestResult result = dispatcher.dispatch(
        client.ctx, aura::ActivateAura{.source = source, .range = 5.0F, .multiplier = 1.2F});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Aura, ActivateAuraRejectedWithoutAnAuraSourcePropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no AuraSource seeded

    request::Dispatcher<aura::ActivateAura> dispatcher;
    dispatcher.register_handler(aura::on_activate_aura);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, aura::ActivateAura{.source = source, .range = 5.0F, .multiplier = 1.2F});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "source has no AuraSource property");
}

TEST(Aura, RefreshAuraEffectAppliesTheMultiplierWhenTargetIsWithinRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);
    server.aura_source_store.set(source, aura::AuraSource{.range = 5.0F, .multiplier = 1.2F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target});

    ASSERT_TRUE(result.accepted);
    // Target is 3.0 units from source, within the 5.0 range: 10 x 1.2 = 12.0.
    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 12.0F);
}

TEST(Aura, RefreshAuraEffectDoesNotApplyWhenTargetIsOutOfRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);
    server.aura_source_store.set(source, aura::AuraSource{.range = 5.0F, .multiplier = 1.2F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target});

    ASSERT_TRUE(result.accepted);
    // Target is 10.0 units away, outside the 5.0 range: unaffected, still 10.0.
    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 10.0F);
}

TEST(Aura, RefreshAuraEffectStopsApplyingOnceTheTargetLeavesRange) {
    // The scenario this mechanism exists to prove: the effect isn't a
    // one-time application that persists until explicitly undone - it's
    // recomputed from scratch every call, so once the target walks out of
    // range, the very next RefreshAuraEffect call reflects that with no
    // separate "remove" step anywhere.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);
    server.aura_source_store.set(source, aura::AuraSource{.range = 5.0F, .multiplier = 1.2F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    ASSERT_TRUE(dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target})
                    .accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 12.0F);

    // Target walks out of range - simulating what a real Move request would
    // have done to Position, without needing to route through one (that
    // arithmetic is movement_test.cpp's job, not aura's).
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});

    ASSERT_TRUE(dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target})
                    .accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 10.0F);
}

TEST(Aura, RefreshAuraEffectAppliesToSelfWhenRangeIsZero) {
    // A range of 0 is not a special case in on_refresh_aura_effect - source
    // is always distance 0.0 from itself, which is always <= 0.0. This is
    // how a self-only buff falls out of the same mechanism a zone effect
    // uses, with no separate code path.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 7.0F, .y = 7.0F});
    server.movement_speed_store.set(source, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, source, 10.0F);
    server.aura_source_store.set(source, aura::AuraSource{.range = 0.0F, .multiplier = 1.5F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = source});

    ASSERT_TRUE(result.accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(source)->get().base, 15.0F);
}

TEST(Aura, RefreshAuraEffectRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef source = client.host.create_entity();
    const EntityRef target = client.host.create_entity();
    client.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    client.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    client.movement_speed_store.set(target, movement::MovementSpeed{.base = 10.0F});
    client.aura_source_store.set(source, aura::AuraSource{.range = 5.0F, .multiplier = 1.2F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, client.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(client.ctx, aura::RefreshAuraEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Aura, RefreshAuraEffectRejectedWithoutAnAuraSourcePropertyOnSource) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no AuraSource seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 10.0F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "source has no AuraSource property");
}

TEST(Aura, RefreshAuraEffectRejectedWithoutPositionOnSource) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no Position seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 10.0F});
    server.aura_source_store.set(source, aura::AuraSource{.range = 5.0F, .multiplier = 1.2F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "source has no Position");
}

TEST(Aura, RefreshAuraEffectRejectedWithoutPositionOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Position seeded
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 10.0F});
    server.aura_source_store.set(source, aura::AuraSource{.range = 5.0F, .multiplier = 1.2F});

    request::Dispatcher<aura::RefreshAuraEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const aura::RefreshAuraEffect& cmd) {
        return aura::on_refresh_aura_effect(ctx, server.movement_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, aura::RefreshAuraEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Position");
}

} // namespace
} // namespace atlas::demo
