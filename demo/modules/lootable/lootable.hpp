#pragma once

// Generated at build time from demo/modules/lootable/lootable.capability.yaml
// (see demo/CMakeLists.txt) - the Lootable/PickUp/PickedUp contracts.
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "lootable.capability.hpp"

namespace atlas::lootable {

// The manual implementation of PickUp's request handler (spec §14, Manual
// Implementation). The second Interactable consumer (issue #237, alongside
// door): rejects without authority (§6), rejects if `item` has no seeded
// Lootable property, and rejects if the item was already collected (§6:
// explicitly reject invalid requests rather than silently coercing a
// no-op into an accept) - unlike door's open/close toggle, a collected item
// never becomes collectible again. On success, flips `collected` in place
// and publishes PickedUp naming both the item and who collected it.
[[nodiscard]] RequestResult on_pick_up(Context& ctx, const PickUp& cmd);

} // namespace atlas::lootable
