#pragma once

// Shared test-support scaffolding for demo/tests/*.cpp - one SimulatedHost
// per server/client in a scenario, hand-composing whichever capabilities
// exist so far (deliberately not the eventual manifest-driven composition,
// matching atlas-runtime's own established scope boundary; see
// demo/README.md). Extracted from combat_scenario_test.cpp once a second
// test file (equipment_test.cpp) needed the exact same host-composition
// boilerplate - not part of demo-capabilities itself, since this is test
// infrastructure, not a capability.
#include "atlas/entity/entity_ref.hpp"
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
#include "movement/movement.hpp"
#include "pathing/pathing.hpp"

namespace atlas::demo::testing {

inline stage::StageSequence make_sequence() {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return std::move(*sequence);
}

// One simulated host - server or client. Each host owns its own property
// storage: a client's copy of Health only ever changes through replication,
// never through locally resolving a request itself (this demo does not
// build client-side prediction, spec §6 - a client here simply waits for
// the server's result).
struct SimulatedHost {
    explicit SimulatedHost(bool has_authority) : host(make_sequence(), has_authority), ctx(host) {
        ctx.register_property_store(health_store);
        ctx.register_property_store(armor_store);
        ctx.register_property_store(position_store);
        ctx.register_property_store(movement_speed_store);
        ctx.register_property_store(path_target_store);
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
    runtime::PropertyStore<movement::Position> position_store;
    runtime::PropertyStore<movement::MovementSpeed> movement_speed_store;
    movement::ContributionRegistry movement_speed_contributions;
    runtime::PropertyStore<pathing::PathTarget> path_target_store;
};

} // namespace atlas::demo::testing
