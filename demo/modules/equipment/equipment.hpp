#pragma once

// Generated at build time from demo/modules/equipment/equipment.capability.yaml
// (see demo/CMakeLists.txt) - the EquipArmor/ArmorEquipped contracts.
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "armor/armor.hpp"
#include "equipment.capability.hpp"

namespace atlas::equipment {

// The manual implementation of EquipArmor's request handler (spec §14,
// Manual Implementation). Contributes to Armor via armor::add_contribution
// - the sanctioned contribution channel (spec §20, Design Rule: "A
// capability must not directly modify another capability's state.
// Contribution is the only channel") - rather than reaching into Armor's
// PropertyStore directly. This is a deliberately different relationship
// than health's to armor: health only *reads* Armor's contract
// (ctx.get<Armor>()), while equipment *contributes to* it
// (armor::add_contribution()) - both legitimate, both declared, neither
// capability reaches into the other's state directly either way.
//
// Takes armor::ContributionRegistry& as an explicit parameter rather than
// routing it through Context: it is per-host bookkeeping owned by whoever
// composes armor into a host (see armor.hpp), the same way a test/host
// composition already passes it explicitly to armor::add_contribution
// itself - equipment's handler is simply the request-driven caller of that
// same API, not a different access path to it.
[[nodiscard]] RequestResult
on_equip_armor(Context& ctx, armor::ContributionRegistry& registry, const EquipArmor& cmd);

} // namespace atlas::equipment
