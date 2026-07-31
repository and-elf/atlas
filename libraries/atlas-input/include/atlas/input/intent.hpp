#pragma once

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
//
// `axis` is a small scalar parameter payload for continuous intents (e.g. how
// far a movement axis is pushed); discrete intents (e.g. "Interact") simply
// leave it at its default. This is a deliberately minimal payload for this
// pass - see this library's README for why a fuller shape (entity target,
// Vec2 axis, matching §19's UI-produced Intent) is left as a follow-up rather
// than guessed at now, since atlas-ui (which also produces this event kind)
// is being designed concurrently in a sibling effort.
//
// A basic aggregate (rule of zero): nothing here protects an invariant across
// its own operations.
struct Intent {
    IntentId id;
    float axis = 0.0F;

    friend constexpr bool operator==(const Intent&, const Intent&) = default;
};

} // namespace atlas::input
