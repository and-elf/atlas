#pragma once

#include "atlas/core/time.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_backend.hpp"

#include <optional>

namespace atlas::render {

// The always-buildable atlas::render::FrameBackend (issue #148): does
// nothing with a Frame's draw commands, satisfying the concept with zero
// third-party dependencies. Exists so the mechanism up to the backend
// boundary (frame building - everything atlas-render already does) stays
// fully buildable and testable in environments with no real GPU or display
// hardware (most CI runners), independent of whichever real backend (#69)
// a given build opts into via ATLAS_RENDER_BACKEND.
//
// "Instantly complete": since this backend performs no actual presentation
// work, the tick it last accepted is, by definition, already fully
// "presented" the moment submit() returns - there is no GPU fence or sync
// object to wait on, so last_completed_tick() simply reports the same tick
// submit() was last called with. An instance that has never been submitted
// to reports std::nullopt, matching last_completed_tick()'s "nothing
// completed yet" case rather than an arbitrary sentinel Time.
//
// A basic aggregate (rule of zero): no invariant beyond ordinary value
// semantics - matches Time/Transform's own precedent of public fields plus
// plain member functions, rather than private state hidden behind
// encapsulation with nothing to protect.
struct NullFrameBackend {
    std::optional<core::Time> last_tick;

    void submit(const Frame& frame) noexcept { last_tick = frame.tick; }

    [[nodiscard]] std::optional<core::Time> last_completed_tick() const noexcept { return last_tick; }
};

static_assert(FrameBackend<NullFrameBackend>);

} // namespace atlas::render
