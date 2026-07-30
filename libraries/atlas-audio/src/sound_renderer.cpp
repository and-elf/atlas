#include "atlas/audio/sound_renderer.hpp"

namespace atlas::audio {

std::vector<ResolvedCue> render(std::span<const EntityRef> voices,
                                const atlas::runtime::PropertyStore<ResourceId>& cues,
                                const atlas::runtime::PropertyStore<float>& gains,
                                const atlas::runtime::PropertyStore<float>& pans) {
    std::vector<ResolvedCue> resolved;
    resolved.reserve(voices.size());

    for (const EntityRef& voice : voices) {
        const auto cue = cues.get(voice);
        const auto gain = gains.get(voice);
        const auto pan = pans.get(voice);
        if (!cue.has_value() || !gain.has_value() || !pan.has_value()) {
            continue;
        }

        resolved.push_back(ResolvedCue{
            .source = voice,
            .cue = cue->get(),
            .effective_gain = clamp_gain(gain->get()),
            .effective_pan = clamp_pan(pan->get()),
        });
    }

    return resolved;
}

} // namespace atlas::audio
