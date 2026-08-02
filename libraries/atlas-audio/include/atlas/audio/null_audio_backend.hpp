#pragma once

#include "atlas/audio/audio_backend.hpp"
#include "atlas/audio/sound_renderer.hpp"

#include <span>

namespace atlas::audio {

// The always-buildable atlas::audio::AudioBackend (issue #149): does
// nothing with a resolved cue list, satisfying the concept with zero
// third-party dependencies. Exists so the mechanism up to the backend
// boundary (cue resolution, mixing parameters - everything atlas-audio
// already does) stays fully buildable and testable in environments with no
// real audio device (most CI runners), independent of whichever real
// backend (#55) a given build opts into via ATLAS_AUDIO_BACKEND.
//
// A basic aggregate (rule of zero): no state at all, since there is
// nothing for a backend that does nothing to track.
struct NullAudioBackend {
    void submit(std::span<const ResolvedCue> /*cues*/) noexcept {}

    void trigger(const TriggeredCue& /*cue*/) noexcept {}
};

static_assert(AudioBackend<NullAudioBackend>);

} // namespace atlas::audio
