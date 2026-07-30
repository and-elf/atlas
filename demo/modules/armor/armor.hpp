#pragma once

// Generated at build time from demo/modules/armor/armor.capability.yaml (see
// demo/CMakeLists.txt) - the Armor property contract, with its composition:
// Additive strategy declared. A separate copy from the stable
// tests/fixtures/armor.capability.yaml atlas-cgen's own tests use to prove
// the generator's composition: support - this demo module's copy is free to
// evolve independently (matching the same reasoning health's own manifest
// copy already established).
#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/property_composition.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "armor.capability.hpp"

namespace atlas::armor {

// This capability's own private per-entity contribution bookkeeping (spec
// §20, Contribution) - not a general-purpose atlas-runtime contribution
// registry; see demo/README.md for why that generalization is deliberately
// out of scope this round. Owned by whoever composes this
// capability into a host (one per host, alongside its PropertyStores) -
// never a namespace-scope global: a global keyed only by EntityRef would
// silently collide between independently-created Host instances, since
// each Host's own EntityRegistry allocates indices starting from 0.
using ContributionRegistry = std::unordered_map<EntityRef, std::vector<runtime::Contribution<std::int32_t>>>;

// Registers a named contribution to entity's Armor in registry and
// immediately re-resolves the Additive-composed effective value back into
// whatever PropertyStore<Armor> ctx has registered - so any other
// capability's ctx.get<Armor>(entity) transparently sees the current
// effective value, with zero awareness that Armor is composed at all (spec
// §20, Design Rule: "Capability B consumes the effective value; it never
// needs to know who contributed to it"). Throws std::logic_error if entity
// has no Armor property seeded yet - contributing to a property that was
// never declared for this entity is a setup mistake, not an ordinary
// outcome.
void add_contribution(Context& ctx,
                      ContributionRegistry& registry,
                      EntityRef entity,
                      std::string_view source,
                      std::int32_t value);

} // namespace atlas::armor
