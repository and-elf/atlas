#pragma once

// Shared host-construction boilerplate for the orb demo's three separate
// process entry points (server_host_main.cpp/client_host_main.cpp/
// editor_host_main.cpp - issue #277). Deliberately mirrors
// demo/tests/simulated_host.hpp's own SimulatedHost shape (real Host+Context,
// generated register_property_stores against a minimal host manifest) but
// lives outside demo/tests/ since it is now real (non-test) application
// scaffolding, and composes only `movement` - the orb has no gameplay
// semantics beyond position/appearance (spec §2/§13: Atlas never
// understands "orbs", only entities/properties/requests; this demo
// composes existing capability contracts rather than inventing a new one,
// per issue #276's own scope).
//
// Each of the three processes constructs its own, entirely separate OrbApp -
// there is no shared memory and, as of this issue, no real transport between
// them either (that is issue #278's own scope) - so today each process's
// orb moves independently. That is this issue's honestly-scoped result, not
// an oversight: #277 is only the process/build split.

#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/renderable.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "movement/movement.hpp"
#include "orb_host.host.hpp"

namespace atlas::demo {

inline stage::StageSequence make_orb_sequence() {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return std::move(*sequence);
}

// has_authority: true only for the server process (spec §6, Server
// Authority) - the client/editor processes construct their own local,
// non-authoritative OrbApp today since there is no replication wiring yet
// (issue #278).
struct OrbApp {
    explicit OrbApp(bool has_authority) : host(make_orb_sequence(), has_authority), ctx(host) {
        register_property_stores(ctx, composition);
    }

    runtime::Host host;
    Context ctx;

    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members) - each
    // reference aliases a member of `composition`, declared immediately
    // above and owned by this same struct for its entire lifetime.
    OrbHost composition;
    runtime::PropertyStore<movement::Position>& position_store = composition.movement_position_store;
    runtime::PropertyStore<movement::MovementSpeed>& movement_speed_store =
        composition.movement_movement_speed_store;
    movement::ContributionRegistry movement_speed_contributions;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    // Renderable (issue #279) is not part of any capability manifest - the
    // orb's appearance, like the existing demo's own player entity
    // (presentation_app.hpp), is a plain PropertyStore read/written
    // directly (renderable_store.get()/.set()), never through ctx.get<T>()
    // or a generated composition member. Recolor/retexture (issue #279)
    // mutates this directly on the authoritative server rather than through
    // a capability request/dispatcher - there is no "appearance" capability
    // manifest to generate one against, and inventing one would be new
    // gameplay semantics the orb epic (#276) explicitly stays out of.
    runtime::PropertyStore<render::Renderable> renderable_store;
};

// A small, fixed palette of material names (issue #279) - real ResourceIds
// (ResourceId::from_name), not placeholder integers, but resolution/
// rendering of what they actually point to stays #280's own scope; this
// issue only proves the recolor/retexture *request* mechanism.
inline constexpr std::array<std::string_view, 3> kOrbMaterialPalette{
    "materials/orb/red",
    "materials/orb/green",
    "materials/orb/blue",
};

// Spawns one orb entity seeded with Position{0, 0}, a base MovementSpeed of
// 4.0 units/second (an arbitrary, small, human-followable speed - no spec/
// design significance), and a default Renderable pointing at the first
// entry of kOrbMaterialPalette (issue #279) - the common seeding every one
// of the three processes needs before it can read, move, or recolor the
// orb.
[[nodiscard]] inline EntityRef spawn_orb(OrbApp& app) {
    const EntityRef orb = app.host.create_entity();
    app.position_store.set(orb, movement::Position{.x = 0.0F, .y = 0.0F});
    app.movement_speed_store.set(orb, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(app.ctx, app.movement_speed_contributions, orb, 4.0F);
    app.renderable_store.set(
        orb, render::Renderable{.mesh = {}, .material = ResourceId::from_name(kOrbMaterialPalette[0])});
    return orb;
}

// Runs app's tick loop at real-time pace (atlas::core::Time::ticks_per_second
// Hz), invoking on_tick once per completed tick with the 1-based tick
// number - shared by all three of this issue's process entry points so the
// same pacing/argv-bound logic (mirroring App::run()'s own shape, app.cpp)
// isn't tripled across them. tick_limit, if set, runs exactly that many
// ticks as fast as possible (no pacing) and returns - the same bounded
// smoke-test mode App::run() offers via --ticks.
void run_paced(OrbApp& app,
               std::optional<std::uint64_t> tick_limit,
               const std::function<void(std::uint64_t)>& on_tick);

} // namespace atlas::demo
