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

TEST(NullAudioBackend, TriggerAcceptsAOneShotCueWithoutFailing) {
    // A one-shot trigger (issue #159: a footstep, a door-open cue) is
    // structurally identical to a ResolvedCue but has no standing value to
    // diff against - unlike submit(), there is no "was this present last
    // tick" question to ask.
    NullAudioBackend backend;
    const TriggeredCue trigger{
        .source = EntityRef{}, .cue = ResourceId::from_name("sfx/door/open"), .gain = 0.8F, .pan = 0.0F};

    backend.trigger(trigger);

    SUCCEED();
}

TEST(NullAudioBackend, RepeatedTriggersAreSafe) {
    NullAudioBackend backend;
    const TriggeredCue trigger{
        .source = EntityRef{}, .cue = ResourceId::from_name("sfx/footstep"), .gain = 0.4F, .pan = -0.5F};

    backend.trigger(trigger);
    backend.trigger(trigger);

    SUCCEED();
}

} // namespace
} // namespace atlas::audio
