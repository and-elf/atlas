// Healing is not its own mechanism (see demo/README.md's "Healing is signed
// damage" section for the full rationale) - it's health::on_apply_damage
// with a negative amount, so these tests exercise on_apply_damage directly,
// the same handler combat_scenario_test.cpp already covers for positive
// (damage) amounts. Kept as its own file rather than folded into
// combat_scenario_test.cpp or health_test.cpp: it's neither the end-to-end
// multi-host replication scenario the former proves, nor the wire-codec
// unit tests the latter is scoped to - it's request-handler behavior for a
// specific input range, the same shape movement_test.cpp uses for on_move.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Healing, NegativeAmountRestoresCurrentHealth) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 5, .maximum = 10});

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = -3});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 8);
}

TEST(Healing, HealIsClampedAtMaximumHealth) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 9, .maximum = 10});

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = -50});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
}

TEST(Healing, HealIsNotMitigatedByArmor) {
    // The behavior this redesign actually exists to prove: Armor mitigates
    // incoming damage, never incoming healing - a target with substantial
    // Armor still receives the full heal amount, unlike ApplyDamage's
    // positive-amount case where the same Armor would blunt the effect
    // (see combat_scenario_test.cpp's DamageExceedingArmorNeverDealsNegativeDamage).
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 5, .maximum = 10});
    server.armor_store.set(target, armor::Armor{.base = 0});
    armor::add_contribution(server.ctx, server.armor_contributions, target, "plate", 999);

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = -3});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 8);
}

TEST(Healing, RejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.health_store.set(target, health::Health{.current = 5, .maximum = 10});

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(client.ctx, health::ApplyDamage{.target = target, .amount = -3});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Healing, RejectsHealingAgainstAnEntityWithNoHealth) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no Health seeded

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = -3});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Health");
}

} // namespace
} // namespace atlas::demo
