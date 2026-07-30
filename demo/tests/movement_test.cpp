// Proves MovementSpeed's Multiplicative composition (spec §20's own worked
// example, "MovementSpeed: 10 x 0.5 (slow) x 1.2 (haste) = 6") end to end
// through a real Move request, the same way combat_scenario_test.cpp proves
// Armor's Additive composition through ApplyDamage - not a unit test of
// resolve_multiplicative alone (tests/atlas-runtime/property_composition_test.cpp
// already covers that in isolation), but this capability's own contribution
// bookkeeping and request handler built on top of it.
#include "atlas/request/dispatch.hpp"
#include "atlas/runtime/property_composition.hpp"

#include <array>
#include <gtest/gtest.h>
#include <stdexcept>

#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Movement, SetBaseSpeedSeedsTheEffectiveValue) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});

    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);

    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 10.0F);
}

TEST(Movement, SetBaseSpeedThrowsWithoutAMovementSpeedPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no MovementSpeed seeded

    EXPECT_THROW(movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F),
                 std::logic_error);
}

TEST(Movement, AddSpeedContributionThrowsWithoutAMovementSpeedPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no MovementSpeed seeded

    EXPECT_THROW(movement::add_speed_contribution(
                     server.ctx, server.movement_speed_contributions, target, "haste", 1.2F),
                 std::logic_error);
}

TEST(Movement, AddSpeedContributionThrowsWithoutABaseSpeedSeededFirst) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.movement_speed_store.set(target,
                                    movement::MovementSpeed{.base = 0.0F}); // seeded, but set_base_speed
                                                                            // never called

    EXPECT_THROW(movement::add_speed_contribution(
                     server.ctx, server.movement_speed_contributions, target, "haste", 1.2F),
                 std::logic_error);
}

TEST(Movement, SpeedContributionsComposeMultiplicatively) {
    // Reproduces spec §20's own Multiplicative example: "MovementSpeed: 10 x
    // 0.5 (slow) x 1.2 (haste) = 6".
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);

    movement::add_speed_contribution(server.ctx, server.movement_speed_contributions, target, "slow", 0.5F);
    movement::add_speed_contribution(server.ctx, server.movement_speed_contributions, target, "haste", 1.2F);

    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 6.0F);
}

TEST(Movement, MoveAdvancesPositionByComposedSpeedOverElapsedTicks) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);
    movement::add_speed_contribution(server.ctx, server.movement_speed_contributions, target, "slow", 0.5F);
    movement::add_speed_contribution(server.ctx, server.movement_speed_contributions, target, "haste", 1.2F);
    // Effective MovementSpeed is now 6.0 (spec §20's own example).

    request::Dispatcher<movement::Move> dispatcher;
    dispatcher.register_handler(movement::on_move);

    // 60 ticks at the default ticks_per_second of 60 is exactly 1 simulated
    // second; moving along +x at speed 6.0 for 1 second covers 6.0 units.
    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        movement::Move{.target = target, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60});

    ASSERT_TRUE(result.accepted);
    const auto position = server.ctx.get<movement::Position>(target);
    ASSERT_TRUE(position.has_value());
    EXPECT_FLOAT_EQ(position->get().x, 6.0F);
    EXPECT_FLOAT_EQ(position->get().y, 0.0F);
}

TEST(Movement, MoveRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F});
    client.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});

    request::Dispatcher<movement::Move> dispatcher;
    dispatcher.register_handler(movement::on_move);

    const RequestResult result = dispatcher.dispatch(
        client.ctx,
        movement::Move{.target = target, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Movement, MoveRejectedWithoutMovementSpeed) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F}); // no MovementSpeed seeded

    request::Dispatcher<movement::Move> dispatcher;
    dispatcher.register_handler(movement::on_move);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        movement::Move{.target = target, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no MovementSpeed");
}

TEST(Movement, RefreshSpeedWithTransientContributionsAppliesWithoutPersisting) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 10.0F);
    movement::add_speed_contribution(server.ctx, server.movement_speed_contributions, target, "haste", 1.2F);
    // Stored (Permanent) effective speed is 10 x 1.2 = 12.0 here.

    const std::array<runtime::Contribution<float>, 1> transient{
        {{.source = "aura", .value = 0.5F, .lifetime = runtime::Lifetime::WhileCondition}}};

    movement::refresh_speed_with_transient_contributions(
        server.ctx, server.movement_speed_contributions, target, transient);

    // 10 x 1.2 (stored) x 0.5 (transient) = 6.0 - folded together in one
    // resolution, never persisted into the registry.
    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 6.0F);

    // Calling again with NO transient contribution falls back to just the
    // stored ones - proving the previous call's transient contribution was
    // never written into the registry (there would be no way to "remove"
    // it if it had been, since nothing fires when an aura's condition stops
    // holding - see property_composition.hpp's Contribution::lifetime).
    movement::refresh_speed_with_transient_contributions(
        server.ctx, server.movement_speed_contributions, target, {});

    EXPECT_FLOAT_EQ(server.ctx.get<movement::MovementSpeed>(target)->get().base, 12.0F);
}

TEST(Movement, RefreshSpeedWithTransientContributionsThrowsWithoutAMovementSpeedPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no MovementSpeed seeded

    EXPECT_THROW(movement::refresh_speed_with_transient_contributions(
                     server.ctx, server.movement_speed_contributions, target, {}),
                 std::logic_error);
}

TEST(Movement, RefreshSpeedWithTransientContributionsThrowsWithoutABaseSpeedSeededFirst) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F}); // seeded, but
                                                                                    // set_base_speed never
                                                                                    // called

    EXPECT_THROW(movement::refresh_speed_with_transient_contributions(
                     server.ctx, server.movement_speed_contributions, target, {}),
                 std::logic_error);
}

TEST(Movement, MoveRejectedWithoutPosition) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F}); // no Position seeded

    request::Dispatcher<movement::Move> dispatcher;
    dispatcher.register_handler(movement::on_move);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        movement::Move{.target = target, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Position");
}

} // namespace
} // namespace atlas::demo
