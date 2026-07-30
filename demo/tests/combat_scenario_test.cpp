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
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <gtest/gtest.h>

#include "armor/armor.hpp"
#include "health/health.hpp"

namespace atlas::demo {
namespace {

stage::StageSequence make_sequence() {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return std::move(*sequence);
}

// One simulated host - server or client - composing the health and armor
// capabilities by hand (deliberately not the eventual manifest-driven
// composition, matching atlas-runtime's own established scope boundary; see
// demo/README.md). Each host owns its own property storage:
// a client's copy of Health only ever changes through replication, never
// through locally resolving ApplyDamage itself (this test does not build
// client-side prediction, spec §6 - a client here simply waits for the
// server's result).
struct SimulatedHost {
    explicit SimulatedHost(bool has_authority) : host(make_sequence(), has_authority), ctx(host) {
        ctx.register_property_store(health_store);
        ctx.register_property_store(armor_store);
    }

    // Simulates replicating this host's current Health for entity to
    // observer - genuinely serialized (health::write_health/read_health,
    // atlas-serialization's ByteWriter/ByteReader), not a shared-memory
    // shortcut, matching spec §7's "Host Communication... in-process
    // calls... test harness integration" (real network transport is
    // explicitly out of scope, spec §13, but the wire encoding itself is
    // not skipped). A method on the source host, not a free function taking
    // two same-type SimulatedHost& parameters - self/observer is
    // unambiguous this way, where two adjacent reference parameters of the
    // same type would be an easily-swapped-by-mistake hazard
    // (bugprone-easily-swappable-parameters).
    void replicate_health_to(SimulatedHost& observer, EntityRef entity) {
        const auto this_health = ctx.get<health::Health>(entity);
        ASSERT_TRUE(this_health.has_value());

        serialization::ByteWriter writer;
        health::write_health(writer, this_health->get());

        serialization::ByteReader reader(writer.bytes());
        const auto decoded = health::read_health(reader);
        ASSERT_TRUE(decoded.has_value());

        observer.health_store.set(entity, *decoded);
    }

    runtime::Host host;
    Context ctx;
    runtime::PropertyStore<health::Health> health_store;
    runtime::PropertyStore<armor::Armor> armor_store;
    armor::ContributionRegistry armor_contributions;
};

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
