#pragma once

// door_hud has no properties/requests/events of its own (see
// door_hud.capability.yaml - depends_on: [door] only), so there is no
// generated door_hud.capability.hpp to include - only door's, for the
// OpenDoor request type this file translates into.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/ui/node.hpp"

#include <optional>

#include "door.capability.hpp"

namespace atlas::door_hud {

// Composes the "Open Door" HUD control (spec §19, Minimum UI Contract) - a
// Node whose Clickable behavior, when invoked, produces an
// atlas::input::Intent{id=OpenDoor, entity=source} - the same event type
// hardware input produces (spec §19: "a button click and a keypress are
// indistinguishable to the capabilities below them"). Takes no door
// parameter: Clickable never stores a target (only which `intent` a click
// produces, matching an InputBinding's own shape), so which door a click
// applies to is the caller's own concern - whichever atlas::EntityRef the
// caller passes as `source` when it later calls Node::try_click, the same
// way `Clickable`'s own doc comment already frames "source" (this mirrors
// there being no NodeId concept yet, see atlas-ui's node.hpp). Pure
// composition, mirroring §19's `health_ui_bridge` worked example's own
// shape: `door` itself is never modified and never told this capability
// exists.
[[nodiscard]] atlas::ui::Node build_open_control();

// Translates a produced Intent into a real door::OpenDoor request, or
// nullopt if the Intent isn't the "OpenDoor" one this capability recognizes
// - the same "only place that knows both" translation role §19's
// `health_ui_bridge.cpp` plays for HealthChanged/UNIT_HEALTH, applied here
// to Intent/request instead of event/Lua. The caller (this demo's own
// pre_tick-style wiring, or a test) is responsible for actually dispatching
// the returned request - this function only decides whether one exists.
[[nodiscard]] std::optional<atlas::door::OpenDoor> to_open_door_request(const atlas::input::Intent& intent);

} // namespace atlas::door_hud
