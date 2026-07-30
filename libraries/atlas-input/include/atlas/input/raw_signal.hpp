#pragma once

#include <concepts>
#include <string_view>
#include <vector>

namespace atlas::input {

// Identifies one raw platform input signal by a semantic, device-independent
// name (e.g. "KeyE", "MouseLeft", "GamepadLeftStickX") - deliberately never a
// device-specific scan code or virtual-key constant, so that even the raw
// side of the boundary stays platform-agnostic vocabulary. This is the raw
// input type the not-yet-built OS backend (§5, Input as Intent - deferred to
// a follow-up issue, see this library's README) will produce and
// IntentRouter resolves against a binding configuration; nothing above that
// resolution step may ever see this type.
//
// A basic aggregate (rule of zero): identity is just the name.
struct RawSignalId {
    std::string_view name;

    friend constexpr bool operator==(const RawSignalId&, const RawSignalId&) noexcept = default;
};

// One raw signal observed during a single poll of a RawSignalSource - the
// sole payload crossing the injectable raw input source seam. `value` is a
// single scalar reading: 1.0 for a plain discrete signal that is currently
// active (e.g. "this key is down"), or a continuous reading for an axis (e.g.
// how far a stick is pushed). This pass does not distinguish an activation
// edge from a continuous update (see README, Scoping decisions) - a signal is
// simply "observed this poll, with this value," which is enough to prove the
// router mechanism without committing to real backend edge semantics yet.
//
// A basic aggregate (rule of zero).
struct RawSignalEvent {
    RawSignalId signal;
    float value = 1.0F;

    friend constexpr bool operator==(const RawSignalEvent&, const RawSignalEvent&) noexcept = default;
};

// The injectable raw input source seam (§5, Input as Intent): anything that
// can be polled once per tick for the raw signals observed since the last
// poll satisfies this concept, with zero inheritance or virtual dispatch -
// consistent with how contract satisfaction works everywhere else in this
// codebase (§5, Tiny Interface Composability). A future OS backend (e.g. an
// SDL3-backed poller) plugs into IntentRouter through this exact concept,
// the same way ScriptedRawSignalSource (this library's fully in-memory,
// deterministic test double) already does.
template <typename T>
concept RawSignalSource = requires(T& source) {
    { source.poll() } -> std::convertible_to<std::vector<RawSignalEvent>>;
};

} // namespace atlas::input
