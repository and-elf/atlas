// Proves pathing's single-target seek: SetPathTarget seeds where an entity
// is heading, and AdvancePathing internally dispatches movement::on_move
// (spec §6, Terminology: Request vs. Internal Dispatch) with a direction
// computed from the entity's current movement::Position toward that target
// - the same "call the owning capability's own function, never touch its
// state directly" discipline equipment_test.cpp already proves for
// equipment::on_equip_armor calling into armor. A real worked scenario
// (start at (0,0), seek (10,0) at 5.0 units/sec) mirrors the way
// movement_test.cpp reproduces spec §20's own numeric example, rather than
// asserting on opaque values.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "pathing/pathing.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Pathing, SetPathTargetSeedsHasTargetAndCoordinates) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.path_target_store.set(
        target, pathing::PathTarget{.target_x = 0.0F, .target_y = 0.0F, .has_target = false});

    request::Dispatcher<pathing::SetPathTarget> dispatcher;
    dispatcher.register_handler(pathing::on_set_path_target);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, pathing::SetPathTarget{.target = target, .target_x = 10.0F, .target_y = 4.0F});

    ASSERT_TRUE(result.accepted);
    const auto path_target = server.ctx.get<pathing::PathTarget>(target);
    ASSERT_TRUE(path_target.has_value());
    EXPECT_TRUE(path_target->get().has_target);
    EXPECT_FLOAT_EQ(path_target->get().target_x, 10.0F);
    EXPECT_FLOAT_EQ(path_target->get().target_y, 4.0F);
}

TEST(Pathing, SetPathTargetRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.path_target_store.set(
        target, pathing::PathTarget{.target_x = 0.0F, .target_y = 0.0F, .has_target = false});

    request::Dispatcher<pathing::SetPathTarget> dispatcher;
    dispatcher.register_handler(pathing::on_set_path_target);

    const RequestResult result = dispatcher.dispatch(
        client.ctx, pathing::SetPathTarget{.target = target, .target_x = 10.0F, .target_y = 4.0F});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Pathing, SetPathTargetRejectedWithoutAPathTargetPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no PathTarget seeded

    request::Dispatcher<pathing::SetPathTarget> dispatcher;
    dispatcher.register_handler(pathing::on_set_path_target);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, pathing::SetPathTarget{.target = target, .target_x = 10.0F, .target_y = 4.0F});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no PathTarget");
}

TEST(Pathing, AdvancePathingMovesTowardTargetAcrossSeveralTicks) {
    // Start at (0,0), seek (10,0) at a plain 5.0 units/sec MovementSpeed (no
    // composition contributions needed here - that arithmetic is already
    // movement_test.cpp's job, not pathing's). Split from the arrival case
    // (AdvancePathingArrivesOnceWithinEpsilon below) purely to keep each
    // test's assertion count focused on one concern.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});
    server.path_target_store.set(
        target, pathing::PathTarget{.target_x = 10.0F, .target_y = 0.0F, .has_target = true});

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    // Tick 1: 60 ticks (1 simulated second) at 5.0 units/sec covers 5.0
    // units - not yet arrived, still 5.0 units short of (10,0).
    ASSERT_TRUE(dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60})
                    .accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::Position>(target)->get().x, 5.0F);
    EXPECT_TRUE(server.ctx.get<pathing::PathTarget>(target)->get().has_target);

    // Tick 2: another 60 ticks covers the remaining 5.0 units exactly,
    // landing precisely on (10,0). Arrival is only detected at the *start*
    // of the next AdvancePathing call (see pathing.cpp) - the tick that
    // lands exactly on the target does not itself clear has_target yet.
    ASSERT_TRUE(dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60})
                    .accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::Position>(target)->get().x, 10.0F);
    EXPECT_TRUE(server.ctx.get<pathing::PathTarget>(target)->get().has_target);
}

TEST(Pathing, AdvancePathingArrivesOnceWithinEpsilon) {
    // Continues where AdvancePathingMovesTowardTargetAcrossSeveralTicks left
    // off: an entity sitting exactly on its PathTarget (distance 0.0, well
    // within the arrival epsilon). The next AdvancePathing call clears
    // has_target and publishes PathTargetReached, with no further movement.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});
    server.path_target_store.set(
        target, pathing::PathTarget{.target_x = 10.0F, .target_y = 0.0F, .has_target = true});

    bool reached_published = false;
    server.ctx.subscribe<pathing::PathTargetReached>(
        [&](const pathing::PathTargetReached& event) { reached_published = event.target == target; });

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60});

    ASSERT_TRUE(result.accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::Position>(target)->get().x, 10.0F);
    EXPECT_FALSE(server.ctx.get<pathing::PathTarget>(target)->get().has_target);
    EXPECT_TRUE(reached_published);
}

TEST(Pathing, AdvancePathingIsANoOpWithoutAnActiveTarget) {
    // has_target == false is a legitimate idle steady state (mirrors
    // health.cpp's treatment of a missing armor::Armor - see pathing.hpp),
    // not a rejection: an entity with nothing to seek can be polled every
    // tick harmlessly.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 1.0F, .y = 2.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});
    server.path_target_store.set(
        target, pathing::PathTarget{.target_x = 0.0F, .target_y = 0.0F, .has_target = false});

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60});

    ASSERT_TRUE(result.accepted);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::Position>(target)->get().x, 1.0F);
    EXPECT_FLOAT_EQ(server.ctx.get<movement::Position>(target)->get().y, 2.0F);
    EXPECT_FALSE(server.ctx.get<pathing::PathTarget>(target)->get().has_target);
}

TEST(Pathing, AdvancePathingRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F});
    client.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});
    client.path_target_store.set(
        target, pathing::PathTarget{.target_x = 10.0F, .target_y = 0.0F, .has_target = true});

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    const RequestResult result =
        dispatcher.dispatch(client.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Pathing, AdvancePathingRejectedWithoutAPathTargetPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no PathTarget seeded
    server.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no PathTarget");
}

TEST(Pathing, AdvancePathingRejectedWithoutPosition) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no Position seeded
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 5.0F});
    server.path_target_store.set(
        target, pathing::PathTarget{.target_x = 10.0F, .target_y = 0.0F, .has_target = true});

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Position");
}

TEST(Pathing, AdvancePathingPropagatesMovementsOwnRejectionWithoutMovementSpeed) {
    // Proves this is a real internal dispatch into movement::on_move (spec
    // §6), not a re-implementation of its validation: pathing never checks
    // MovementSpeed itself, so a missing one surfaces as movement's own
    // rejection reason, unchanged.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F}); // no MovementSpeed seeded
    server.path_target_store.set(
        target, pathing::PathTarget{.target_x = 10.0F, .target_y = 0.0F, .has_target = true});

    request::Dispatcher<pathing::AdvancePathing> dispatcher;
    dispatcher.register_handler(pathing::on_advance_pathing);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, pathing::AdvancePathing{.target = target, .delta_ticks = 60});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no MovementSpeed");
}

} // namespace
} // namespace atlas::demo
