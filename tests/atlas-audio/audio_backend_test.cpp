#include "atlas/audio/audio_backend.hpp"
#include "atlas/audio/null_audio_backend.hpp"

#include <gtest/gtest.h>

namespace atlas::audio {
namespace {

static_assert(AudioBackend<NullAudioBackend>);

TEST(NullAudioBackend, SubmitAcceptsAnEmptySpanWithoutFailing) {
    NullAudioBackend backend;

    backend.submit({});

    SUCCEED();
}

TEST(NullAudioBackend, SubmitIgnoresCueContentsWithoutFailing) {
    // "Does nothing with a std::span" (issue #149) proven by handing it a
    // genuinely non-empty cue list rather than only ever exercising it with
    // an empty span, which would never distinguish "ignores contents" from
    // "would break given real ones".
    NullAudioBackend backend;
    const std::vector<ResolvedCue> cues{
        ResolvedCue{
            .source = EntityRef{}, .cue = ResourceId{}, .effective_gain = 0.5F, .effective_pan = -0.25F},
        ResolvedCue{
            .source = EntityRef{}, .cue = ResourceId{}, .effective_gain = 1.0F, .effective_pan = 1.0F},
    };

    backend.submit(cues);

    SUCCEED();
}

TEST(NullAudioBackend, RepeatedSubmitsAreSafe) {
    NullAudioBackend backend;
    const std::vector<ResolvedCue> cues{
        ResolvedCue{
            .source = EntityRef{}, .cue = ResourceId{}, .effective_gain = 0.2F, .effective_pan = 0.0F},
    };

    backend.submit(cues);
    backend.submit({});
    backend.submit(cues);

    SUCCEED();
}

} // namespace
} // namespace atlas::audio
