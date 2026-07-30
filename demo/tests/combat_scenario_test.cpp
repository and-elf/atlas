// True integration test, not a unit test: composes two independent,
// mutually-unaware capabilities (health, armor - generated via atlas-cgen
// from demo/modules/health/ and demo/modules/armor/) into three
// hand-composed hosts (server, and two observing clients), and drives the
// exact scenario spec §21's worked example describes end to end: a
// client-issued ApplyDamage request, server-side validation and property
// composition (Armor mitigating the incoming damage, spec §20), authoritative
// mutation, and replication of the result to both observing clients over a
// real (in-process, but genuinely serialized) wire - proving the request,
// property-composition, and replication mechanisms actually work together,
// not just individually. See demo/README.md for the scope boundary this
// test deliberately stays inside (no manifest-driven capability composition,
// no client-side prediction, no real network transport).
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(CombatScenario, ArmorMitigatedDamageReplicatesIdenticallyToBothObservingClients) {
    SimulatedHost server{/*has_authority=*/true};
    SimulatedHost client_a{/*has_authority=*/false};
    SimulatedHost client_b{/*has_authority=*/false};

    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 10, .maximum = 10});
    server.armor_store.set(target, armor::Armor{.base = 0});
    armor::add_contribution(server.ctx, server.armor_contributions, target, "plate", 5);

    // Client A issues the request; the server is the only host that ever
    // dispatches it, matching §6 (Server Authority) - a client here never
    // resolves ApplyDamage against its own (non-authoritative) copy.
    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const health::ApplyDamage request{.target = target, .amount = 10};
    const RequestResult result = dispatcher.dispatch(server.ctx, request);

    ASSERT_TRUE(result.accepted);

    // 10 incoming damage, 5 armor: 5 dealt (spec §20's own Additive example,
    // "Armor: 100 + 50 + 20 = 170", applied here as mitigation instead of
    // accumulation - same mechanism, different arithmetic use of it).
    const auto server_health = server.ctx.get<health::Health>(target);
    ASSERT_TRUE(server_health.has_value());
    EXPECT_EQ(server_health->get().current, 5);

    server.replicate_health_to(client_a, target);
    server.replicate_health_to(client_b, target);

    const auto client_a_health = client_a.ctx.get<health::Health>(target);
    const auto client_b_health = client_b.ctx.get<health::Health>(target);
    ASSERT_TRUE(client_a_health.has_value());
    ASSERT_TRUE(client_b_health.has_value());
    EXPECT_EQ(client_a_health->get().current, 5);
    EXPECT_EQ(client_b_health->get().current, 5);
}

TEST(CombatScenario, ClientIssuedRequestIsRejectedForLackingAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.health_store.set(target, health::Health{.current = 10, .maximum = 10});

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(client.ctx, health::ApplyDamage{.target = target, .amount = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(CombatScenario, DamageExceedingArmorNeverDealsNegativeDamage) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 10, .maximum = 10});
    server.armor_store.set(target, armor::Armor{.base = 0});
    armor::add_contribution(server.ctx, server.armor_contributions, target, "fortress plate", 999);

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = 10});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 10);
}

TEST(CombatScenario, RejectsApplyDamageAgainstAnEntityWithNoHealth) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no Health seeded

    request::Dispatcher<health::ApplyDamage> dispatcher;
    dispatcher.register_handler(health::on_apply_damage);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = 10});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "target has no Health");
}

} // namespace
} // namespace atlas::demo
