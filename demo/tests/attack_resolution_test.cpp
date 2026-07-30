// Proves attack_resolution::resolve_targeted_attack - the range +
// line-of-sight + damage-application sequence factored out of
// auto_attack::on_try_auto_attack once it became clear every targeted
// attack or spell (a future instant-attack request, a future spell cast)
// would need the exact same sequence. Called directly, not through
// request::Dispatcher, the same "a real internal dispatch, not a
// re-implementation" shape health::on_apply_damage and
// line_of_sight::blocks_line_of_sight already establish for functions meant
// to be composed into a request handler rather than dispatched themselves.
#include <gtest/gtest.h>

#include "attack_resolution/attack_resolution.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(AttackResolution, LandsWhenInRangeAndUnobstructed) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_TRUE(outcome.result.accepted);
    EXPECT_TRUE(outcome.landed);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
}

TEST(AttackResolution, ValidNoOpWhenTargetIsBeyondMaxRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_TRUE(outcome.result.accepted);
    EXPECT_FALSE(outcome.landed);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
}

TEST(AttackResolution, ValidNoOpWhenTargetIsWithinMinRange) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 1.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 2.0F,
                                               .max_range = 10.0F,
                                               .damage = 10});

    EXPECT_TRUE(outcome.result.accepted);
    EXPECT_FALSE(outcome.landed);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
}

TEST(AttackResolution, ValidNoOpWhenLineOfSightIsBlocked) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 1.5F, .center_y = 0.0F, .radius = 1.0F});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = obstacle,
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_TRUE(outcome.result.accepted);
    EXPECT_FALSE(outcome.landed);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 20);
}

TEST(AttackResolution, NoObstacleSkipsTheLineOfSightCheckEntirely) {
    // obstacle.is_null() means "nothing to check" (the same EntityRef::is_null()
    // convention auto_attack::TryAutoAttack already uses), not "an obstacle
    // that happens to block nothing."
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 20, .maximum = 20});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_TRUE(outcome.landed);
}

TEST(AttackResolution, RejectsWithoutPositionOnAttacker) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity(); // no Position seeded
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_FALSE(outcome.result.accepted);
    EXPECT_EQ(outcome.result.rejection_reason, "attacker has no Position");
    EXPECT_FALSE(outcome.landed);
}

TEST(AttackResolution, RejectsWithoutPositionOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Position seeded
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_FALSE(outcome.result.accepted);
    EXPECT_EQ(outcome.result.rejection_reason, "target has no Position");
    EXPECT_FALSE(outcome.landed);
}

TEST(AttackResolution, PropagatesHealthsOwnRejectionWithoutHealthOnTarget) {
    // Proves this is a real internal dispatch into health::on_apply_damage
    // (spec §6), not a re-implementation of its validation - attack_resolution
    // never checks Health itself, so a missing one surfaces as health's own
    // rejection reason, unchanged.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef attacker = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Health seeded
    server.position_store.set(attacker, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});

    const auto outcome = attack_resolution::resolve_targeted_attack(
        server.ctx,
        attack_resolution::TargetedAttackQuery{.attacker = attacker,
                                               .target = target,
                                               .obstacle = EntityRef{},
                                               .min_range = 0.0F,
                                               .max_range = 5.0F,
                                               .damage = 10});

    EXPECT_FALSE(outcome.result.accepted);
    EXPECT_EQ(outcome.result.rejection_reason, "target has no Health");
    EXPECT_FALSE(outcome.landed);
}

} // namespace
} // namespace atlas::demo
