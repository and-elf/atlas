// Proves the cyclic melee/ranged auto-attack: TryAutoAttack, driven
// explicitly each call (delta_ticks), lands only when off cooldown, in
// [min_range, max_range] of the target, and unobstructed
// (line_of_sight::blocks_line_of_sight, via
// attack_resolution::resolve_targeted_attack - see attack_resolution_test.cpp
// for that shared sequence's own dedicated coverage; this file only proves
// on_try_auto_attack wires it up correctly plus the cooldown gating that
// stays auto_attack-specific) - the same "caller simulates the tick"
// pattern movement_test.cpp/aura_test.cpp/pathing_test.cpp already
// establish. Also proves the two ability shapes from the original design
// conversation: QueueAttackBonus enhances the next landed swing without
// attacking itself (a Heroic Strike shape), and a plain health::ApplyDamage
// dispatch needs no auto_attack involvement at all to deal instant damage
// (a Sinister Strike shape) - InstantAttackBypassesTheAutoAttackCooldownEntirely
// proves the two mechanisms are genuinely independent, not just documented
// as such.
//
// The swing cycle's own lifecycle (atlas::runtime::ActionState, in
// server.weapon_action_registry - see atlas-runtime's own README section)
// replaced bare cooldown-value inspection for cancellation purposes.
// movement::PositionChanged/interruption::ActionInterrupted are triggered
// properties (spec §20, Triggered composition; issue #47) - on_try_auto_attack
// reads them via ordinary ctx.get<T>(attacker), right before calling
// atlas::runtime::request_cancel/advance_action, as attacker's own
// scheduled turn to notice a cancellation. There is no separate
// subscription/callback mechanism at all: the same call both notices the
// trigger and (via advance_action) applies its full-cycle cooldown penalty
// (the same two-step "notice, then apply on the same advance" shape
// cast_time_attack_test.cpp documents at more length). Unlike
// cast_time_attack, a cancelled cycle restarts immediately (action_state
// moves back to Started, not left at Cancelled) - the swing cycle is
// perpetual, there is no "go idle" state for it.
#include "atlas/request/dispatch.hpp"
#include "atlas/runtime/action.hpp"

#include <gtest/gtest.h>

#include "auto_attack/auto_attack.hpp"
#include "interruption/interruption.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(AutoAttack, QueueAttackBonusAccumulatesOntoPendingBonusDamage) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    bool landed_published = false;
    std::int32_t published_damage = 0;
    server.ctx.subscribe<auto_attack::AutoAttackLanded>([&](const auto_attack::AutoAttackLanded& event) {
        landed_published = event.attacker == attacker && event.target == target;
        published_damage = event.damage;
    });

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::QueueAttackBonus> bonus_dispatcher;
    bonus_dispatcher.register_handler(auto_attack::on_queue_attack_bonus);
    ASSERT_TRUE(
        bonus_dispatcher
            .dispatch(server.ctx, auto_attack::QueueAttackBonus{.attacker = attacker, .bonus_damage = 4})
            .accepted);

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 50,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 2,
                                                             .max_range = 10,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, client.weapon_action_registry, cmd);
    });

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
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "attacker has no Position");
}

TEST(AutoAttack, TryAutoAttackAcceptsAsNoOpWhileOnCooldownEvenWithoutPositionSeeded) {
    // Position validation happens inside attack_resolution::resolve_targeted_attack,
    // which on_try_auto_attack only calls once off cooldown - proving that
    // ordering directly, not just documenting it: a still-on-cooldown swing
    // is a valid no-op regardless of whether the attacker/target could even
    // be resolved yet.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity(); // no Position seeded
    const EntityRef target = server.host.create_entity();   // no Position seeded
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 50,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        auto_attack::TryAutoAttack{
            .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 40);
}

TEST(AutoAttack, TryAutoAttackRejectedWithoutPositionOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Position seeded
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

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
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 45,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});

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

TEST(AutoAttack,
     TryAutoAttackAppliesTheFullCyclePenaltyWhenMovementTriggeredAndTheWeaponRequiresStandingStill) {
    // movement::PositionChanged is read directly (spec §20, Triggered
    // composition) as attacker's own scheduled turn, right before
    // advance_action runs - no separate subscription/callback involved.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 20,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = true});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = false};
    server.ctx.set<movement::PositionChanged>(attacker,
                                              movement::PositionChanged{.new_x = 1.0F, .new_y = 0.0F});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              auto_attack::TryAutoAttack{.attacker = attacker,
                                                         .target = EntityRef{},
                                                         .obstacle = EntityRef{},
                                                         .delta_ticks = 10})
                    .accepted);

    // Full-cycle penalty, not the ordinary 10-tick decrement - the cycle
    // restarts immediately (action_state back to Started).
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 60);
    EXPECT_EQ(server.weapon_action_registry.at(attacker).action_state, runtime::ActionState::Started);
}

TEST(AutoAttack, TryAutoAttackTicksNormallyWhenMovementTriggeredButTheWeaponDoesNotRequireStandingStill) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 20,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = false};
    server.ctx.set<movement::PositionChanged>(attacker,
                                              movement::PositionChanged{.new_x = 1.0F, .new_y = 0.0F});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              auto_attack::TryAutoAttack{.attacker = attacker,
                                                         .target = EntityRef{},
                                                         .obstacle = EntityRef{},
                                                         .delta_ticks = 10})
                    .accepted);

    // The ordinary 10-tick decrement, not the full-cycle penalty - a weapon
    // that opted out of requires_stationary is unaffected by movement.
    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 10);
}

TEST(AutoAttack, TryAutoAttackIsUnaffectedByAnUnrelatedEntitysMovement) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef bystander = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 20,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = true});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = false};
    // bystander's own PositionChanged slot is entirely independent of
    // attacker's - ctx.get<PositionChanged>(attacker) below sees nullopt.
    server.ctx.set<movement::PositionChanged>(bystander,
                                              movement::PositionChanged{.new_x = 1.0F, .new_y = 0.0F});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              auto_attack::TryAutoAttack{.attacker = attacker,
                                                         .target = EntityRef{},
                                                         .obstacle = EntityRef{},
                                                         .delta_ticks = 10})
                    .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 10);
}

TEST(AutoAttack,
     TryAutoAttackAppliesTheFullCyclePenaltyWhenActionInterruptedTriggeredRegardlessOfRequiresStationary) {
    // The generic mechanism: unlike movement, this ignores requires_stationary
    // entirely - being stunned interrupts any weapon's swing-in-progress.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 20,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = false};
    server.ctx.set<interruption::ActionInterrupted>(attacker, interruption::ActionInterrupted{});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              auto_attack::TryAutoAttack{.attacker = attacker,
                                                         .target = EntityRef{},
                                                         .obstacle = EntityRef{},
                                                         .delta_ticks = 10})
                    .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 60);
    EXPECT_EQ(server.weapon_action_registry.at(attacker).action_state, runtime::ActionState::Started);
}

TEST(AutoAttack, TryAutoAttackIsUnaffectedByAnUnrelatedEntitysActionInterrupted) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef bystander = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 20,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = false});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = false};
    server.ctx.set<interruption::ActionInterrupted>(bystander, interruption::ActionInterrupted{});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              auto_attack::TryAutoAttack{.attacker = attacker,
                                                         .target = EntityRef{},
                                                         .obstacle = EntityRef{},
                                                         .delta_ticks = 10})
                    .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 10);
}

TEST(AutoAttack, TryAutoAttackDoesNotPenalizeAnAlreadyReadyWeaponEvenWithATriggeredCancellation) {
    // cooldown_remaining_ticks == 0 means nothing is in progress - a
    // cancellation applied while ready must never itself impose a delay.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 0,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = true});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Started, .cancel_requested = false};
    server.ctx.set<movement::PositionChanged>(attacker,
                                              movement::PositionChanged{.new_x = 1.0F, .new_y = 0.0F});

    request::Dispatcher<auto_attack::TryAutoAttack> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              auto_attack::TryAutoAttack{.attacker = attacker,
                                                         .target = EntityRef{},
                                                         .obstacle = EntityRef{},
                                                         .delta_ticks = 10})
                    .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 0);
}

TEST(AutoAttack, DispatchingMoveThenTryAutoAttackAppliesTheCooldownPenaltyEndToEnd) {
    // Proves movement::on_move's ctx.set<PositionChanged>(...) actually
    // connects to auto_attack::on_try_auto_attack's own ctx.get<...> read -
    // no subscription/callback wiring involved at all (unlike before issue
    // #47).
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(attacker, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, attacker, 10.0F);
    server.weapon_attack_store.set(attacker,
                                   auto_attack::WeaponAttack{.min_range = 0,
                                                             .max_range = 5,
                                                             .attack_speed_ticks = 60,
                                                             .damage = 10,
                                                             .cooldown_remaining_ticks = 20,
                                                             .pending_bonus_damage = 0,
                                                             .requires_stationary = true});
    server.weapon_action_registry[attacker] =
        auto_attack::WeaponAction{.action_state = runtime::ActionState::Ongoing, .cancel_requested = false};

    request::Dispatcher<movement::Move> move_dispatcher;
    move_dispatcher.register_handler(movement::on_move);
    ASSERT_TRUE(
        move_dispatcher
            .dispatch(server.ctx,
                      movement::Move{
                          .target = attacker, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60})
            .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 20);

    request::Dispatcher<auto_attack::TryAutoAttack> attack_dispatcher;
    attack_dispatcher.register_handler([&](Context& ctx, const auto_attack::TryAutoAttack& cmd) {
        return auto_attack::on_try_auto_attack(ctx, server.weapon_action_registry, cmd);
    });
    ASSERT_TRUE(
        attack_dispatcher
            .dispatch(server.ctx,
                      auto_attack::TryAutoAttack{
                          .attacker = attacker, .target = target, .obstacle = EntityRef{}, .delta_ticks = 5})
            .accepted);

    EXPECT_EQ(server.ctx.get<auto_attack::WeaponAttack>(attacker)->get().cooldown_remaining_ticks, 60);
}

} // namespace
} // namespace atlas::demo
