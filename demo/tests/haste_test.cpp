// Proves haste's range-based effect on cast_time_attack::CastSpeed: the
// CastSpeed analogue of aura_test.cpp's proof for movement::MovementSpeed.
// ActivateHaste seeds a source's declared range/multiplier, and
// RefreshHasteEffect - the per-tick re-evaluation a WhileCondition
// contribution needs (spec §20; see
// cast_time_attack::refresh_cast_speed_with_transient_contributions and
// aura's own module section for why this can never be a stored,
// incrementally added/removed entry) - applies the multiplier only while
// the target is within range, recomputed fresh on every call rather than
// persisted.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "haste/haste.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Haste, ActivateHasteSeedsRangeAndMultiplier) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    server.haste_source_store.set(source, haste::HasteSource{.range = 0, .multiplier = 1.0F});

    request::Dispatcher<haste::ActivateHaste> dispatcher;
    dispatcher.register_handler(haste::on_activate_haste);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, haste::ActivateHaste{.source = source, .range = 5, .multiplier = 1.3F});

    ASSERT_TRUE(result.accepted);
    const auto haste_source = server.ctx.get<haste::HasteSource>(source);
    ASSERT_TRUE(haste_source.has_value());
    EXPECT_EQ(haste_source->get().range, 5);
    EXPECT_FLOAT_EQ(haste_source->get().multiplier, 1.3F);
}

TEST(Haste, ActivateHasteRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef source = client.host.create_entity();
    client.haste_source_store.set(source, haste::HasteSource{.range = 0, .multiplier = 1.0F});

    request::Dispatcher<haste::ActivateHaste> dispatcher;
    dispatcher.register_handler(haste::on_activate_haste);

    const RequestResult result = dispatcher.dispatch(
        client.ctx, haste::ActivateHaste{.source = source, .range = 5, .multiplier = 1.3F});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Haste, ActivateHasteRejectedWithoutAHasteSourcePropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no HasteSource seeded

    request::Dispatcher<haste::ActivateHaste> dispatcher;
    dispatcher.register_handler(haste::on_activate_haste);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, haste::ActivateHaste{.source = source, .range = 5, .multiplier = 1.3F});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "source has no HasteSource property");
}

TEST(Haste, RefreshHasteEffectAppliesTheMultiplierWhenTargetIsWithinRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.haste_source_store.set(source, haste::HasteSource{.range = 5, .multiplier = 1.3F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, server.cast_speed_store, server.cast_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target});

    ASSERT_TRUE(result.accepted);
    // No stored contributions, target within the 5.0 range: 1.0 x 1.3 = 1.3.
    EXPECT_FLOAT_EQ(server.ctx.get<cast_time_attack::CastSpeed>(target)->get().base, 1.3F);
}

TEST(Haste, RefreshHasteEffectDoesNotApplyWhenTargetIsOutOfRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.haste_source_store.set(source, haste::HasteSource{.range = 5, .multiplier = 1.3F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, server.cast_speed_store, server.cast_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target});

    ASSERT_TRUE(result.accepted);
    // Out of range: falls back to no haste at all (the declared identity,
    // 1.0), the same fallback BeginCast itself treats a missing CastSpeed
    // property as.
    EXPECT_FLOAT_EQ(server.ctx.get<cast_time_attack::CastSpeed>(target)->get().base, 1.0F);
}

TEST(Haste, RefreshHasteEffectStopsApplyingOnceTheTargetLeavesRange) {
    // The scenario this mechanism exists to prove: the effect isn't a
    // one-time application that persists until explicitly undone - it's
    // recomputed from scratch every call, so once the target walks out of
    // range, the very next RefreshHasteEffect call reflects that with no
    // separate "remove" step anywhere.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.haste_source_store.set(source, haste::HasteSource{.range = 5, .multiplier = 1.3F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, server.cast_speed_store, server.cast_speed_contributions, cmd);
    });

    ASSERT_TRUE(dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target})
                    .accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<cast_time_attack::CastSpeed>(target)->get().base, 1.3F);

    // Target walks out of range - simulating what a real Move request would
    // have done to Position, without needing to route through one.
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});

    ASSERT_TRUE(dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target})
                    .accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<cast_time_attack::CastSpeed>(target)->get().base, 1.0F);
}

TEST(Haste, RefreshHasteEffectRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef source = client.host.create_entity();
    const EntityRef target = client.host.create_entity();
    client.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    client.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    client.haste_source_store.set(source, haste::HasteSource{.range = 5, .multiplier = 1.3F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, client.cast_speed_store, client.cast_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(client.ctx, haste::RefreshHasteEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Haste, RefreshHasteEffectRejectedWithoutAHasteSourcePropertyOnSource) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no HasteSource seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, server.cast_speed_store, server.cast_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "source has no HasteSource property");
}

TEST(Haste, RefreshHasteEffectRejectedWithoutPositionOnSource) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no Position seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.haste_source_store.set(source, haste::HasteSource{.range = 5, .multiplier = 1.3F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, server.cast_speed_store, server.cast_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "source has no Position");
}

TEST(Haste, RefreshHasteEffectRejectedWithoutPositionOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Position seeded
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.haste_source_store.set(source, haste::HasteSource{.range = 5, .multiplier = 1.3F});

    request::Dispatcher<haste::RefreshHasteEffect> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const haste::RefreshHasteEffect& cmd) {
        return haste::on_refresh_haste_effect(
            ctx, server.cast_speed_store, server.cast_speed_contributions, cmd);
    });

    const RequestResult result =
        dispatcher.dispatch(server.ctx, haste::RefreshHasteEffect{.source = source, .target = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Position");
}

} // namespace
} // namespace atlas::demo
