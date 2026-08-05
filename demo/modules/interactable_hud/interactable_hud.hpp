#pragma once

// interactable_hud has no properties/requests/events of its own (see
// interactable_hud.capability.yaml - depends_on: [interactable] only), so
// there is no generated interactable_hud.capability.hpp - only
// interactable's, for the Interactable contract this file reads.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/ui/node.hpp"

#include <optional>

#include "interactable.capability.hpp"

namespace atlas::interactable_hud {

// Composes a generic HUD control (spec §19, Minimum UI Contract) for any
// entity carrying an interactable::Interactable property - the
// generalization of door_hud's original, door-specific build_open_control
// (issue #237, following review discussion on #235): reads `entity`'s own
// `action`/`designator` and builds one Node whose `resource` is the
// designator (a Text-resource reference a real backend renders, spec §3)
// and whose Clickable produces that entity's own configured `action`
// Intent when clicked - the same Node shape door_hud used to hand-build
// per-door, now built once, generically, for door, lootable, or any future
// interactable entity type. Returns nullopt for an entity with no
// Interactable property at all, matching every other capability's
// established "missing property is an ordinary outcome, not an error"
// convention (e.g. health.cpp reading a target's absent Armor) - not every
// entity in the world needs to be clickable.
//
// This function never knows what "OpenDoor" or "PickUp" mean (spec §2,
// Mechanism Over Meaning) - translating the click's resulting Intent back
// into a real, typed request is each composing capability's own small
// bridge (door_hud::to_open_door_request, lootable_hud::to_pick_up_request),
// never this one's job.
[[nodiscard]] std::optional<atlas::ui::Node> build_control(atlas::Context& ctx, atlas::EntityRef entity);

} // namespace atlas::interactable_hud
