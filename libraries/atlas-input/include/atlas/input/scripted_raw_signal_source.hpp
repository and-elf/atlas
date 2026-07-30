#pragma once

#include "atlas/input/raw_signal.hpp"

#include <cstddef>
#include <vector>

namespace atlas::input {

// A fully in-memory, deterministic RawSignalSource test double - a scripted
// queue of raw-signal frames, one frame consumed per poll() call, in order.
// This is what makes IntentRouter testable without real hardware (§4:
// simulation-affecting logic must never read OS/hardware entropy directly),
// and is exactly the seam a future OS backend (SDL3 or similar, deferred -
// see this library's README) plugs into instead of this type.
//
// An encapsulated class rather than a plain aggregate: "each poll() call
// advances through the script exactly once, never replaying or skipping a
// frame" is a real invariant this type protects across its own poll() calls,
// the same reasoning atlas::core::Random gives for encapsulating its engine
// cursor rather than exposing it as a public field.
class ScriptedRawSignalSource {
public:
    explicit ScriptedRawSignalSource(std::vector<std::vector<RawSignalEvent>> scripted_frames);

    // Returns the next scripted frame, or an empty frame once the script is
    // exhausted - never throws. A host polling more ticks than a test
    // scripted for is an entirely ordinary situation (e.g. "no more input
    // happens for the rest of the test"), not an error condition.
    [[nodiscard]] std::vector<RawSignalEvent> poll();

private:
    std::vector<std::vector<RawSignalEvent>> frames_;
    std::size_t next_frame_ = 0;
};

} // namespace atlas::input
