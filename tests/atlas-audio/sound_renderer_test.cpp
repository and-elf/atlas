#include "atlas/audio/sound_renderer.hpp"
#include "atlas/runtime/property_composition.hpp"

#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace atlas::audio {
namespace {

using atlas::EntityRef;
using atlas::ResourceId;
using atlas::runtime::PropertyStore;

TEST(Render, NoVoicesProducesNoResolvedCues) {
    const PropertyStore<ResourceId> cues;
    const PropertyStore<float> gains;
    const PropertyStore<float> pans;

    const auto resolved = render({}, cues, gains, pans);

    EXPECT_TRUE(resolved.empty());
}

TEST(Render, SingleFullyComposedVoiceResolvesItsCueGainAndPan) {
    const EntityRef voice{1, 0};
    const ResourceId cue = ResourceId::from_name("sfx/footstep");

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    cues.set(voice, cue);
    gains.set(voice, 0.75F);
    pans.set(voice, -0.25F);

    const std::array<EntityRef, 1> voices{voice};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 1U);
    EXPECT_EQ(resolved[0].source, voice);
    EXPECT_EQ(resolved[0].cue, cue);
    EXPECT_FLOAT_EQ(resolved[0].effective_gain, 0.75F);
    EXPECT_FLOAT_EQ(resolved[0].effective_pan, -0.25F);
}

TEST(Render, PreservesCallerSuppliedVoiceOrderRegardlessOfStorageOrder) {
    const EntityRef first{1, 0};
    const EntityRef second{2, 0};

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    // Populate the stores in the opposite order from the requested playback
    // order below - render() must follow the caller-supplied `voices` span,
    // never PropertyStore's internal (unordered_map) iteration order (spec
    // §4: avoid unordered iteration wherever it could affect output).
    cues.set(second, ResourceId::from_name("sfx/second"));
    gains.set(second, 1.0F);
    pans.set(second, 0.0F);
    cues.set(first, ResourceId::from_name("sfx/first"));
    gains.set(first, 1.0F);
    pans.set(first, 0.0F);

    const std::array<EntityRef, 2> voices{first, second};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 2U);
    EXPECT_EQ(resolved[0].source, first);
    EXPECT_EQ(resolved[1].source, second);
}

TEST(Render, VoiceMissingAnyComposedPropertyIsSkippedNotDefaulted) {
    const EntityRef voice_without_cue{1, 0};
    const EntityRef voice_without_gain{2, 0};
    const EntityRef voice_without_pan{3, 0};
    const EntityRef fully_composed{4, 0};

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    gains.set(voice_without_cue, 1.0F);
    pans.set(voice_without_cue, 0.0F);

    cues.set(voice_without_gain, ResourceId::from_name("sfx/b"));
    pans.set(voice_without_gain, 0.0F);

    cues.set(voice_without_pan, ResourceId::from_name("sfx/c"));
    gains.set(voice_without_pan, 1.0F);

    cues.set(fully_composed, ResourceId::from_name("sfx/d"));
    gains.set(fully_composed, 1.0F);
    pans.set(fully_composed, 0.0F);

    const std::array<EntityRef, 4> voices{
        voice_without_cue, voice_without_gain, voice_without_pan, fully_composed};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 1U);
    EXPECT_EQ(resolved[0].source, fully_composed);
}

TEST(Render, ClampsGainAboveOneDownToOne) {
    const EntityRef voice{1, 0};

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    cues.set(voice, ResourceId::from_name("sfx/loud"));
    gains.set(voice, 4.5F);
    pans.set(voice, 0.0F);

    const std::array<EntityRef, 1> voices{voice};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 1U);
    EXPECT_FLOAT_EQ(resolved[0].effective_gain, 1.0F);
}

TEST(Render, ClampsNegativeGainUpToZero) {
    const EntityRef voice{1, 0};

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    cues.set(voice, ResourceId::from_name("sfx/negative"));
    gains.set(voice, -2.0F);
    pans.set(voice, 0.0F);

    const std::array<EntityRef, 1> voices{voice};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 1U);
    EXPECT_FLOAT_EQ(resolved[0].effective_gain, 0.0F);
}

TEST(Render, ClampsPanBeyondFullLeftOrRight) {
    const EntityRef left_voice{1, 0};
    const EntityRef right_voice{2, 0};

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    cues.set(left_voice, ResourceId::from_name("sfx/left"));
    gains.set(left_voice, 1.0F);
    pans.set(left_voice, -8.0F);
    cues.set(right_voice, ResourceId::from_name("sfx/right"));
    gains.set(right_voice, 1.0F);
    pans.set(right_voice, 8.0F);

    const std::array<EntityRef, 2> voices{left_voice, right_voice};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 2U);
    EXPECT_FLOAT_EQ(resolved[0].effective_pan, -1.0F);
    EXPECT_FLOAT_EQ(resolved[1].effective_pan, 1.0F);
}

TEST(Render, RepeatedCallsWithIdenticalInputsProduceBitIdenticalOutput) {
    // Determinism regression guard (spec §4): identical composed input
    // state must render to identical output every time, with no dependency
    // on wall-clock time, hidden global state, or map-iteration order.
    const EntityRef first{1, 0};
    const EntityRef second{2, 0};

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    cues.set(first, ResourceId::from_name("sfx/first"));
    gains.set(first, 0.6F);
    pans.set(first, 0.3F);
    cues.set(second, ResourceId::from_name("sfx/second"));
    gains.set(second, 0.9F);
    pans.set(second, -0.4F);

    const std::array<EntityRef, 2> voices{first, second};

    const auto first_run = render(voices, cues, gains, pans);
    const auto second_run = render(voices, cues, gains, pans);

    ASSERT_EQ(first_run.size(), second_run.size());
    for (std::size_t index = 0; index < first_run.size(); ++index) {
        EXPECT_EQ(first_run[index], second_run[index]);
    }
}

TEST(ClampGain, ClampsToTheZeroToOneRange) {
    EXPECT_FLOAT_EQ(clamp_gain(-1.0F), 0.0F);
    EXPECT_FLOAT_EQ(clamp_gain(0.5F), 0.5F);
    EXPECT_FLOAT_EQ(clamp_gain(2.0F), 1.0F);
}

TEST(ClampPan, ClampsToTheMinusOneToOneRange) {
    EXPECT_FLOAT_EQ(clamp_pan(-2.0F), -1.0F);
    EXPECT_FLOAT_EQ(clamp_pan(0.1F), 0.1F);
    EXPECT_FLOAT_EQ(clamp_pan(2.0F), 1.0F);
}

TEST(Render, ConsumesGainComposedThroughTheRealPropertyCompositionEngine) {
    // Integration proof (per issue #31): atlas-audio never recomposes
    // contributions itself - composition is atlas-runtime's job (§20). This
    // confirms the intended pipeline actually works end to end: a
    // capability composes an effective gain via
    // atlas::runtime::resolve_multiplicative over real Contribution<float>
    // values, stores the resolved effective value in a PropertyStore, and
    // only then does the audio renderer consume it.
    const EntityRef voice{1, 0};
    const std::array<atlas::runtime::Contribution<float>, 2> gain_contributions{{
        {.source = "ducking", .value = 0.5F},
        {.source = "settings_volume", .value = 1.2F},
    }};
    const float composed_gain = atlas::runtime::resolve_multiplicative<float>(1.0F, gain_contributions);

    PropertyStore<ResourceId> cues;
    PropertyStore<float> gains;
    PropertyStore<float> pans;
    cues.set(voice, ResourceId::from_name("sfx/composed"));
    gains.set(voice, composed_gain);
    pans.set(voice, 0.0F);

    const std::array<EntityRef, 1> voices{voice};
    const auto resolved = render(voices, cues, gains, pans);

    ASSERT_EQ(resolved.size(), 1U);
    EXPECT_FLOAT_EQ(resolved[0].effective_gain, 0.6F);
}

TEST(ResolvedCue, EqualityComparesEveryFieldNotJustTheFirstDifference) {
    // ResolvedCue's operator== is defaulted (rule of zero); this exercises
    // the "not equal" branch for each field independently, not just the
    // all-equal happy path RepeatedCallsWithIdenticalInputsProduceBitIdenticalOutput
    // above already covers.
    const ResolvedCue base{.source = EntityRef{1, 0},
                           .cue = ResourceId::from_name("sfx/a"),
                           .effective_gain = 0.5F,
                           .effective_pan = 0.0F};

    EXPECT_EQ(base, base);
    EXPECT_NE(base,
              (ResolvedCue{.source = EntityRef{2, 0},
                           .cue = base.cue,
                           .effective_gain = base.effective_gain,
                           .effective_pan = base.effective_pan}));
    EXPECT_NE(base,
              (ResolvedCue{.source = base.source,
                           .cue = ResourceId::from_name("sfx/b"),
                           .effective_gain = base.effective_gain,
                           .effective_pan = base.effective_pan}));
    EXPECT_NE(base,
              (ResolvedCue{.source = base.source,
                           .cue = base.cue,
                           .effective_gain = 0.9F,
                           .effective_pan = base.effective_pan}));
    EXPECT_NE(base,
              (ResolvedCue{.source = base.source,
                           .cue = base.cue,
                           .effective_gain = base.effective_gain,
                           .effective_pan = 0.9F}));
}

} // namespace
} // namespace atlas::audio
