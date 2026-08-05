#pragma once

// Generated at build time from
// demo/modules/interactable/interactable.capability.yaml (see
// demo/CMakeLists.txt) - the Interactable contract. No requests/events, and
// no hand-written .cpp: this capability is purely a shared, generic
// vocabulary property (spec §237's own generalization of door_hud) -
// "which action does clicking this entity produce, and what should a HUD
// call it."
//
// `action: IntentId` names the semantic intent a click on this entity
// produces - the same IntentId vocabulary atlas-input's IntentRouter and
// atlas-ui's Clickable already use (see interactable_hud, which reads this
// field to build a real Clickable). `designator: ResourceId` is what a HUD
// shows the player (spec §3, Resource: never a hardcoded string literal) -
// it reuses atlas::ui::Node::resource's existing generic resource-reference
// slot exactly, the same mechanism §19 already uses for icon/background/
// font references, applied here to a "Text" resource kind for i18n (a
// locale's actual string bytes behind a given ResourceId are a load-time/
// host concern - which locale's blob the host's ResourceRegistry loaded -
// never something this property or interactable_hud decide).
//
// Deliberately capability-agnostic, the same way `interruption`'s
// ActionInterrupted is: `door`, `lootable`, or any future clickable entity
// type composes this property without `interactable`/`interactable_hud`
// knowing or caring what "OpenDoor" or "PickUp" actually mean (spec §2,
// Mechanism Over Meaning) - translating a produced Intent back into a real,
// typed request is each composing capability's own small bridge (door_hud,
// lootable_hud), not this capability's job.
#include "interactable.capability.hpp"
