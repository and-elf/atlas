#pragma once

#include "atlas/input/raw_signal.hpp"

#include <vector>

namespace atlas::input {

// The always-buildable atlas::input::RawSignalSource (issue #68): reports
// no raw signals, ever, satisfying the concept with zero third-party
// dependencies. Exists so the mechanism up to the backend boundary
// (IntentRouter, binding resolution - everything atlas-input already does)
// stays fully buildable and testable in environments with no real
// keyboard/mouse/gamepad access, independent of whichever real backend
// (Sdl3RawSignalSource, this same issue) a given build opts into via
// ATLAS_INPUT_BACKEND.
//
// Distinct from ScriptedRawSignalSource: that type is a test double
// requiring a pre-authored script and is exhausted after replaying it (see
// its own doc comment); this type is a genuine zero-configuration
// production backend for a build that wants no input source at all,
// mirroring atlas::render::NullFrameBackend/atlas::audio::NullAudioBackend.
//
// A basic aggregate (rule of zero): no state at all, since there is
// nothing for a source that reports nothing to track.
struct NullRawSignalSource {
    // Kept as an instance method for consistency with every sibling
    // NullXBackend's own submit()/trigger(), all called via source.poll()-
    // shaped instance syntax (concept-generic callers like IntentRouter's
    // templated poll() never know the concrete type, so "static" is
    // meaningless from their call site) - matching them beats a
    // technically-static-but-inconsistent method just to satisfy this check.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] std::vector<RawSignalEvent> poll() { return {}; }
};

static_assert(RawSignalSource<NullRawSignalSource>);

} // namespace atlas::input
