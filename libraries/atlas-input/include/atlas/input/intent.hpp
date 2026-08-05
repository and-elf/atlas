#pragma once

#include "atlas/entity/entity_ref.hpp"

#include <string_view>

namespace atlas::input {

// Identifies one semantic, game-declared action a player (or, per §19, a UI
// interaction, or a Lua macro producing the same event type) has expressed -
// e.g. "Interact", "MoveForward", "PrimaryAction" (spec §5, Input as Intent).
// This is the *only* input-shaped type a capability author ever sees; it
// never names a key, button, or device.
//
// A basic aggregate (rule of zero), the same pattern as
// atlas::stage::StageId: identity is just the name, so no constructor is
// needed to protect anything. `name` is expected to reference a string with
// static storage duration (a string literal or a binding config's own
// long-lived storage), matching how every other semantic-name vocabulary
// type in this codebase is defined.
struct IntentId {
    std::string_view name;

    friend constexpr bool operator==(const IntentId&, const IntentId&) noexcept = default;
};

// One semantic action, resolved from raw input by IntentRouter against a
// binding configuration - the sole event type crossing the boundary §5
// (Input as Intent) draws between raw platform input and capability code.
// Also the type atlas-ui's Clickable behavior produces from a UI click (spec
// §19: "the same Intent events atlas-input produces from hardware input" -
// see atlas::ui::Clickable::invoke), matching §5's own documented shape
// (`events: Intent: { kind: IntentId, entity: EntityRef, axis:
// optional<Vec2> }`) except for `axis`, which stays a plain float rather
// than becoming `optional<Vec2>` - no `Vec2` type exists anywhere in this
// codebase yet (see this library's README, Open Questions), so this pass
// does not invent one.
//
// `entity` names which entity expressed this intent - the polling entity for
// hardware input (IntentRouter::poll's caller-supplied parameter), or the
// Clickable's own `source` for a UI-produced Intent. `axis` is a small
// scalar parameter payload for continuous intents (e.g. how far a movement
// axis is pushed); discrete intents (e.g. "Interact") simply leave it at its
// default.
//
// A basic aggregate (rule of zero): nothing here protects an invariant across
// its own operations.
struct Intent {
    IntentId id;
    atlas::EntityRef entity{};
    float axis = 0.0F;

    friend constexpr bool operator==(const Intent&, const Intent&) = default;
};

} // namespace atlas::input
