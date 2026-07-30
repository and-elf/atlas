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
// not the 11 individual ctx.register_property_store(...) calls that used
// to live in this constructor. Event-subscription wiring (the
// ctx.subscribe<...> calls below) is still hand-written - manifest-driven
// wiring for *that* is real future work, not yet built (see
// docs/specification/14-generated-contracts.md and this repo's issue #12).
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

        // Wires the generic cancellation mechanism (see
        // demo/README.md's "Interrupting an in-progress action" section):
        // every capability with cancellable in-progress state (auto_attack's
        // swing timer, cast_time_attack's wind-up) subscribes to
        // movement::PositionChanged (opt-in, per its own requires_stationary
        // flag) and interruption::ActionInterrupted (unconditional). This is
        // host-composition wiring - deciding *which* capabilities react to
        // *which* events - the same kind of decision the generated
        // PropertyStore registration above already is, not something either
        // capability decides for itself; unlike that registration, this
        // wiring isn't generated yet (see this file's own header comment).
        ctx.subscribe<movement::PositionChanged>([this](const movement::PositionChanged& event) {
            auto_attack::on_movement_occurred(ctx, weapon_action_registry, event);
            cast_time_attack::on_movement_occurred(ctx, cast_action_registry, event);
        });
        ctx.subscribe<interruption::ActionInterrupted>([this](const interruption::ActionInterrupted& event) {
            auto_attack::on_action_interrupted(weapon_action_registry, event);
            cast_time_attack::on_action_interrupted(cast_action_registry, event);
        });
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
