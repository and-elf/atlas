#pragma once

// lootable_hud has no properties/requests/events of its own (see
// lootable_hud.capability.yaml - depends_on: [lootable] only), so there is
// no generated lootable_hud.capability.hpp - only lootable's, for the
// PickUp request type this file translates into.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"

#include <optional>

#include "lootable.capability.hpp"

namespace atlas::lootable_hud {

// Translates a produced Intent into a real lootable::PickUp request, or
// nullopt if the Intent isn't the "PickUp" one this capability recognizes -
// the same translation role door_hud::to_open_door_request plays for
// door::OpenDoor (issue #237's second Interactable consumer, proving the
// translation pattern generalizes across genuinely different request
// shapes). `collector` is not part of the clicked Node's own Intent at all
// (Intent carries only the clicked entity - the item - as `entity`) - it is
// the caller's own concern, the same way Node::try_click's own `source`
// parameter is caller-supplied rather than inferred, so this function takes
// it explicitly rather than guessing at it.
[[nodiscard]] std::optional<atlas::lootable::PickUp> to_pick_up_request(const atlas::input::Intent& intent,
                                                                        atlas::EntityRef collector);

} // namespace atlas::lootable_hud
