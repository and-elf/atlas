#pragma once

// door_hud has no properties/requests/events of its own (see
// door_hud.capability.yaml - depends_on: [door] only), so there is no
// generated door_hud.capability.hpp to include - only door's, for the
// OpenDoor request type this file translates into.
#include "atlas/input/intent.hpp"

#include <optional>

#include "door.capability.hpp"

namespace atlas::door_hud {

// Translates a produced Intent into a real door::OpenDoor request, or
// nullopt if the Intent isn't the "OpenDoor" one this capability recognizes
// - the same "only place that knows both" translation role §19's
// `health_ui_bridge.cpp` plays for HealthChanged/UNIT_HEALTH, applied here
// to Intent/request instead of event/Lua. The caller (this demo's own
// pre_tick-style wiring, or a test) is responsible for actually dispatching
// the returned request - this function only decides whether one exists.
//
// This is now the *entire* contents of door_hud (issue #237, following
// review discussion on #235): the Node-building half
// (build_open_control(), as this capability originally had it) is now
// interactable_hud::build_control() - one generic function shared by every
// interactable entity type, not one hand-written Node per capability. door
// itself composes interactable::Interactable (seeded with `action =
// IntentId{"OpenDoor"}`) rather than door_hud hardcoding a Node - door_hud's
// only remaining job is knowing that its own "OpenDoor" IntentId maps to a
// real door::OpenDoor request, which is inherently door-specific and not
// something a generic mechanism could do on door's behalf.
[[nodiscard]] std::optional<atlas::door::OpenDoor> to_open_door_request(const atlas::input::Intent& intent);

} // namespace atlas::door_hud
