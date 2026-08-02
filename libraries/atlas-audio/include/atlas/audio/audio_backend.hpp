#pragma once

#include "atlas/audio/sound_renderer.hpp"

#include <concepts>
#include <span>

namespace atlas::audio {

// The compile-time contract (spec §5: "checked like a C++ concept, never a
// runtime interface table or virtual dispatch lookup") every audio backend -
// real or null - must satisfy. Which concrete type actually gets compiled
// into a given build is a configure-time CMake choice (ATLAS_AUDIO_BACKEND,
// see libraries/atlas-audio/CMakeLists.txt), never a runtime factory or
// plugin lookup (spec §4). Mirrors atlas-render's FrameBackend (issue #148)
// exactly, adapted to this library's own output shape.
//
// Unlike Frame (which bundles its simulation tick alongside the draw
// commands it carries), render()'s std::vector<ResolvedCue> carries no
// per-tick correlator - issue #149 explicitly scopes this to "no change to
// render()'s own signature or behavior" - so there is nothing here
// analogous to FrameBackend::last_completed_tick() to report; submit() is
// the entire contract.
template <typename T>
concept AudioBackend = requires(T& backend, std::span<const ResolvedCue> cues) {
    { backend.submit(cues) } -> std::same_as<void>;
};

} // namespace atlas::audio
