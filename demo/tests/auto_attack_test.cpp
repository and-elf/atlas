// Proves the cyclic melee/ranged auto-attack: TryAutoAttack, driven
// explicitly each call (delta_ticks), lands only when off cooldown, in
// [min_range, max_range] of the target, and unobstructed
// (line_of_sight::blocks_line_of_sight) - the same "caller simulates the
// tick" pattern movement_test.cpp/aura_test.cpp/pathing_test.cpp already
// establish. Also proves the two ability shapes from the original design
// conversation: QueueAttackBonus enhances the next landed swing without
// attacking itself (a Heroic Strike shape), and a plain health::ApplyDamage
// dispatch needs no auto_attack involvement at all to deal instant damage
// (a Sinister Strike shape) - InstantAttackBypassesTheAutoAttackCooldownEntirely
// proves the two mechanisms are genuinely independent, not just documented
// as such.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "auto_attack/auto_attack.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(AutoAttack, QueueAttackBonusAccumulatesOntoPendingBonusDamage) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::QueueAttackBonus> dispatcher;
    dispatcher.register_handler(auto_attack::on_queue_attack_bonus);

    ASSERT_TRUE(
        dispatcher
            .dispatch(server.ctx, auto_attack::QueueAttackBonus{.attacker = attacker, .bonus_damage = 5})
            .accepted);
    ASSERT_TRUE(
        dispatcher
            .dispatch(server.ctx, auto_attack::QueueAttackBonus{.attacker = attacker, .bonus_damage = 3})
            .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().pending_bonus_damage, 8);
}

TEST(AutoAttack, QueueAttackBonusRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef attacker = client.host.create_entity();
    client.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::QueueAttackBonus> dispatcher;
    dispatcher.register_handler(auto_attack::on_queue_attack_bonus);

    const RequestResult result = dispatcher.dispatch(
        client.ctx, auto_attack::QueueAttackBonus{.attacker = attacker, .bonus_damage = 5});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(AutoAttack, QueueAttackBonusRejectedWithoutAWeaponAttackPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity(); // no WeaponAttack seeded

    request::Dispatcher<auto_attack::QueueAttackBonus> dispatcher;
    dispatcher.register_handler(auto_attack::on_queue_attack_bonus);

    const RequestResult result = dispatcher.dispatch(
        server.ctx, auto_attack::QueueAttackBonus{.attacker = attacker, .bonus_damage = 5});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "attacker has no WeaponAttack property");
}

TEST(AutoAttack, TryAutoAttackLandsWhenOffCooldownInRangeAndUnobstructed) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    bool landed_published = false;
    std::int32_t published_damage = 0;
    server.ctx.subscribe<auto_attack::AutoAttackLanded>([&](const auto_attack::AutoAttackLanded& event) {
        landed_published = event.attacker == attacker && event.target == target;
        published_damage = event.damage;
    });

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 60);
    EXPECT_TRUE(landed_published);
    EXPECT_EQ(published_damage, 10);
}

TEST(AutoAttack, TryAutoAttackConsumesPendingBonusDamageOnLanding) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::QueueAttackBonus> bonus_dispatcher;
    bonus_dispatcher.register_handler(auto_attack::on_queue_attack_bonus);
    ASSERT_TRUE(
        bonus_dispatcher
            .dispatch(server.ctx, auto_attack::QueueAttackBonus{.attacker = attacker, .bonus_damage = 4})
            .accepted);

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    ASSERT_TRUE(
        dispatcher
            .dispatch(server.ctx,
                      auto_attack::TryAutoAttack{
                          .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10})
            .accepted);

    // Base damage 10 + queued bonus 4 = 14 dealt in one landed swing.
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 6);
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().pending_bonus_damage, 0);
}

TEST(AutoAttack, TryAutoAttackPropagatesHealthsOwnRejectionWithoutHealthOnTarget) {
    // Proves this is a real internal dispatch into health::on_apply_damage
    // (spec §6), not a re-implementation of its validation: auto_attack
    // never checks Health itself, so a missing one surfaces as health's own
    // rejection reason, unchanged - and, since that rejection is checked
    // before the cooldown/pending_bonus_damage reset, a swing that didn't
    // actually connect never consumes either.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Health seeded
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Health");
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 0);
}

TEST(AutoAttack, TryAutoAttackDoesNotLandWhileOnCooldown) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 50,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 40);
}

TEST(AutoAttack, TryAutoAttackDoesNotLandWhenTargetIsBeyondMaxRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
    // Still ready (0), waiting for the target to come into range - not
    // reset to attack_speed_ticks, since no swing actually landed.
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 0);
}

TEST(AutoAttack, TryAutoAttackDoesNotLandWhenTargetIsWithinMinRange) {
    // A ranged weapon's minimum range in effect - proves min_range/max_range
    // are two independent bounds, not just a maximum.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 1.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 2.0F,
                                                             .max_range = 10.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
}

TEST(AutoAttack, TryAutoAttackDoesNotLandWhenLineOfSightIsBlocked) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 1.5F, .center_y = 0.0F, .radius = 1.0F});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = obstacle, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
}

TEST(AutoAttack, TryAutoAttackRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef attacker = client.host.create_entity();
    const EntityRef target = client.host.create_entity();
    client.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    client.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    client.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        client.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(AutoAttack, TryAutoAttackRejectedWithoutAWeaponAttackPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity(); // no WeaponAttack seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "attacker has no WeaponAttack property");
}

TEST(AutoAttack, TryAutoAttackRejectedWithoutPositionOnAttacker) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity(); // no Position seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "attacker has no Position");
}

TEST(AutoAttack, TryAutoAttackRejectedWithoutPositionOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Position seeded
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler(auto_attack::on_try_auto_attack);

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Position");
}

TEST(AutoAttack, InstantAttackBypassesTheAutoAttackCooldownEntirely) {
    // The "Sinister Strike" shape: a plain health::ApplyDamage dispatch,
    // entirely independent of WeaponAttack - proving this class of ability
    // needs no involvement from auto_attack at all, not just asserting it.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0.0F,
                                                             .max_range = 5.0F,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 45,
                                                             .pending_bonus_damage = 0});

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = 7});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 13);
    // Bit-for-bit unchanged - an instant attack never touches auto_attack's
    // own state at all.
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 45);
}

} // namespace
} // namespace atlas::demo
