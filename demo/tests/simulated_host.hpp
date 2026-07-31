#pragma once

// Shared test-support scaffolding for demo/tests/*.cpp - one SimulatedHost
// per server/client in a scenario. Extracted from combat_scenario_test.cpp
// once a second test file (equipment_test.cpp) needed the exact same
// host-composition boilerplate - not part of demo-capabilities itself,
// since this is test infrastructure, not a capability.
//
// PropertyStore registration is generated (spec §14, atlas-cgen's host
// composition mode - see simulated_host.host.yaml and DemoHost/
// register_property_stores below) rather than hand-written: composition
// only had to hand-write which capabilities to compose (the host manifest),
// not the individual ctx.register_property_store(...)/
// ctx.register_triggered_property_store(...) calls that used to live in
// this constructor. movement::PositionChanged/interruption::ActionInterrupted
// used to need hand-wired ctx.subscribe<...> registrations here (issue #12) -
// as of issue #47 they're triggered properties instead, read via ordinary
// ctx.get<T>() by whichever capability's own scheduled turn cares (see
// auto_attack::on_try_auto_attack/cast_time_attack::on_advance_cast), so
// there is no subscription left to wire up at all.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/replication/property_codec.hpp"
#include "atlas/replication/property_id.hpp"
#include "atlas/replication/property_id_codec.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <gtest/gtest.h>

#include "armor/armor.hpp"
#include "aura/aura.hpp"
#include "auto_attack/auto_attack.hpp"
#include "cast_time_attack/cast_time_attack.hpp"
#include "damage_over_time/damage_over_time.hpp"
#include "haste/haste.hpp"
#include "health/health.hpp"
#include "interruption/interruption.hpp"
#include "line_of_sight/line_of_sight.hpp"
#include "movement/movement.hpp"
#include "pathing/pathing.hpp"
#include "simulated_host.host.hpp"

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
        register_property_stores(ctx, composition);
    }

    // Simulates replicating this host's current Health for entity to
    // observer - genuinely serialized as a generic (PropertyId, fields)
    // wire tuple (atlas::replication::write_property_id/write_property_fields,
    // issue #18), not a shared-memory shortcut, matching spec §7's "Host
    // Communication... in-process calls... test harness integration" (real
    // network transport is explicitly out of scope, spec §13, but the wire
    // encoding itself is not skipped). PropertyId::from_name("Health") is
    // opaque to this call site in spirit - the server side of a real
    // replication frame would never interpret it - it's read back and
    // compared here only because this test wants to prove the id round-
    // trips, which health::write_health/read_health (still used directly by
    // health_test.cpp) had no equivalent of: that hand-written codec never
    // put a property identity on the wire at all, only the two int32
    // fields. A method on the source host, not a free function taking two
    // same-type SimulatedHost& parameters - self/observer is unambiguous
    // this way, where two adjacent reference parameters of the same type
    // would be an easily-swapped-by-mistake hazard
    // (bugprone-easily-swappable-parameters).
    void replicate_health_to(SimulatedHost& observer, EntityRef entity) {
        const auto this_health = ctx.get<health::Health>(entity);
        ASSERT_TRUE(this_health.has_value());

        const auto health_property_id = PropertyId::from_name("Health");

        serialization::ByteWriter writer;
        replication::write_property_id(writer, health_property_id);
        replication::write_property_fields(writer, this_health->get());

        serialization::ByteReader reader(writer.bytes());
        const auto decoded_id = replication::read_property_id(reader);
        ASSERT_TRUE(decoded_id.has_value());
        EXPECT_EQ(*decoded_id, health_property_id);

        const auto decoded = replication::read_property_fields<health::Health>(reader);
        ASSERT_TRUE(decoded.has_value());

        observer.health_store.set(entity, *decoded);
    }

    runtime::Host host;
    Context ctx;

    // Generated (see this file's own header comment): every
    // runtime::PropertyStore<T> below is an alias into one of composition's
    // own members, never a second, separate store - composition holds the
    // real storage, registered against ctx via register_property_stores
    // above. Kept as named references (composition must be declared first:
    // member initialization follows declaration order) purely so every
    // existing call site below and across demo/tests/*.cpp
    // (server.health_store.set(...), etc.) needed no renaming just because
    // registration itself moved to generated code - composition's own
    // generated member names (e.g. health_health_store) are an
    // implementation detail this struct's public surface doesn't expose.
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members) - each
    // reference aliases a member of `composition`, declared immediately
    // above and owned by this same struct for its entire lifetime; there is
    // no dangling risk, and SimulatedHost is never copied or reassigned.
    DemoHost composition;
    runtime::PropertyStore<health::Health>& health_store = composition.health_health_store;
    runtime::PropertyStore<armor::Armor>& armor_store = composition.armor_armor_store;
    armor::ContributionRegistry armor_contributions;
    runtime::PropertyStore<movement::Position>& position_store = composition.movement_position_store;
    runtime::PropertyStore<movement::MovementSpeed>& movement_speed_store =
        composition.movement_movement_speed_store;
    movement::ContributionRegistry movement_speed_contributions;
    runtime::PropertyStore<pathing::PathTarget>& path_target_store = composition.pathing_path_target_store;
    runtime::PropertyStore<aura::AuraSource>& aura_source_store = composition.aura_aura_source_store;
    runtime::PropertyStore<line_of_sight::Obstacle>& obstacle_store =
        composition.line_of_sight_obstacle_store;
    runtime::PropertyStore<auto_attack::WeaponAttack>& weapon_attack_store =
        composition.auto_attack_weapon_attack_store;
    auto_attack::ActionRegistry weapon_action_registry;
    runtime::PropertyStore<cast_time_attack::CastTimeAttack>& cast_time_attack_store =
        composition.cast_time_attack_cast_time_attack_store;
    cast_time_attack::ActionRegistry cast_action_registry;
    runtime::PropertyStore<haste::HasteSource>& haste_source_store = composition.haste_haste_source_store;
    runtime::PropertyStore<haste::CastSpeed>& cast_speed_store = composition.haste_cast_speed_store;
    runtime::PropertyStore<damage_over_time::DotEffect>& dot_effect_store =
        composition.damage_over_time_dot_effect_store;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};

} // namespace atlas::demo::testing
