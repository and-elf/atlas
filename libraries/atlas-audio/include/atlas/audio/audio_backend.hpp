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
// Two independent requirements, matching spec §20's Continuous vs.
// Triggered Composition split rather than one undifferentiated cue list
// (issue #159):
//
// - submit() delivers this tick's full sustained/standing voice snapshot
//   (ResolvedCue's own doc comment spells out the stop-on-absence diffing
//   contract this implies - a documented obligation, not something this
//   concept can check structurally). Unlike Frame (which bundles its
//   simulation tick alongside the draw commands it carries), the
//   std::vector<ResolvedCue> this hands over carries no per-tick correlator,
//   so there is nothing here analogous to FrameBackend::last_completed_tick()
//   to report.
// - trigger() fires one independent one-shot voice per call, with no
//   standing value and no diffing - it always plays to completion
//   regardless of any subsequent submit() snapshot.
template <typename T>
concept AudioBackend = requires(T& backend, std::span<const ResolvedCue> cues, const TriggeredCue& trigger) {
    { backend.submit(cues) } -> std::same_as<void>;
    { backend.trigger(trigger) } -> std::same_as<void>;
};

} // namespace atlas::audio
