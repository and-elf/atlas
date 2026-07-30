#pragma once

// Generated at build time from demo/modules/movement/movement.capability.yaml
// (see demo/CMakeLists.txt) - the Position/MovementSpeed/Move/PositionChanged
// contracts, with MovementSpeed's composition: Multiplicative strategy
// declared.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/property_composition.hpp"

#include <string_view>
#include <unordered_map>
#include <vector>

#include "movement.capability.hpp"

namespace atlas::movement {

// This capability's own private per-entity contribution bookkeeping (spec
// §20, Contribution), mirroring armor::ContributionRegistry's role but not
// its shape: armor::add_contribution can safely hardcode Additive's
// identity (0) as its resolution's starting point because Armor's declared
// base happens to already equal that identity value (armor.cpp explains
// this). MovementSpeed's Multiplicative composition has no such luck -
// Multiplicative's identity is 1.0, not a property's actual declared base
// (e.g. 10.0 for an ordinary walking speed), so resolving from a hardcoded
// 1.0 would silently discard the real base on every contribution. Instead,
// each entity's declared base is recorded once, explicitly, in
// declared_base, and every later resolution reads it back from here - never
// from PropertyStore<MovementSpeed>, which by definition already holds the
// *previous* resolution's output rather than the original declared value.
struct SpeedContributions {
    float declared_base = 0.0F;
    std::vector<runtime::Contribution<float>> contributions;
};

// Owned by whoever composes this capability into a host (one per host,
// alongside its PropertyStores) - never a namespace-scope global, for the
// same reason armor::ContributionRegistry isn't one (see armor.hpp).
using ContributionRegistry = std::unordered_map<EntityRef, SpeedContributions>;

// Seeds entity's declared base MovementSpeed: recorded in registry (so a
// later add_speed_contribution call has a real base to resolve from) and
// written straight through to ctx's PropertyStore<MovementSpeed> as the
// initial effective value, since with zero contributions the effective
// value equals the base exactly (spec §20's own Multiplicative example
// degenerates to this when there is nothing left to multiply by). The
// one-time setup step every entity with a composed MovementSpeed needs
// before any contribution can be added.
void set_base_speed(Context& ctx, ContributionRegistry& registry, EntityRef entity, float base_speed);

// Registers a named Multiplicative contribution to entity's MovementSpeed
// and immediately re-resolves the effective value back into whatever
// PropertyStore<MovementSpeed> ctx has registered, from the entity's tracked
// declared_base (see SpeedContributions above for why that source matters
// here specifically, unlike Additive) - so any other capability's
// ctx.get<MovementSpeed>(entity) transparently sees the current effective
// value (spec §20, Design Rule). Throws std::logic_error if entity has no
// base speed seeded yet via set_base_speed.
void add_speed_contribution(Context& ctx,
                            ContributionRegistry& registry,
                            EntityRef entity,
                            std::string_view source,
                            float multiplier);

// Removes source's contribution to entity's MovementSpeed - the reverse of
// add_speed_contribution - and re-resolves the effective value from scratch
// against the entity's tracked declared_base and whatever contributions
// remain, never against PropertyStore<MovementSpeed>'s current value (the
// same reason add_speed_contribution already resolves from declared_base -
// see SpeedContributions above). This is what a WhileCondition-lifetime
// contribution (spec §20; e.g. an aura whose target just left range) needs
// on the way out, the same way add_speed_contribution is what it needs on
// the way in. Throws std::logic_error under the same two conditions
// add_speed_contribution already does (no MovementSpeed property seeded, no
// base speed seeded via set_base_speed); removing a source that was never
// actually added is a harmless no-op, matching
// atlas::runtime::remove_contributions_by_source's own contract.
void remove_speed_contribution(Context& ctx,
                               ContributionRegistry& registry,
                               EntityRef entity,
                               std::string_view source);

// The manual implementation of Move's request handler (spec §14). Advances
// entity's Position along the (direction_x, direction_y) direction at its
// current effective MovementSpeed, over delta_ticks simulation ticks (spec
// §4: deterministic tick-based time - atlas::core::Time::ticks_per_second,
// never wall-clock time). direction_x/direction_y are expected pre-
// normalized by the caller: Atlas resolves raw input into a semantic Intent
// upstream of any capability (spec §5, Input as Intent) - this handler is
// never where "was W held down" becomes a unit vector, only where an
// already-semantic movement intent becomes a Position update. Rejects if
// entity has no Position or no MovementSpeed - unlike Armor (legitimately
// optional per spec §20's own health.cpp precedent), an entity issuing Move
// is expected to have both, so a missing one is a setup mistake, not an
// ordinary no-mitigation case.
[[nodiscard]] RequestResult on_move(Context& ctx, const Move& cmd);

} // namespace atlas::movement
