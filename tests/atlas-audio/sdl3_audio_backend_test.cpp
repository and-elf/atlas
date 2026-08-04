#include "atlas/audio/audio_backend.hpp"
#include "atlas/audio/decode_cache.hpp"
#include "atlas/audio/sdl3_audio_backend.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::audio {
namespace {

static_assert(AudioBackend<Sdl3AudioBackend>);

// SDL_AUDIODRIVER=dummy is set as this test binary's own process environment
// (tests/atlas-audio/CMakeLists.txt, via gtest_discover_tests' ENVIRONMENT
// property) rather than set here per-test via SDL_SetHint - matching
// tests/atlas-input/sdl3_raw_signal_source_test.cpp's own "offscreen" video
// driver approach in spirit, except the audio driver has to be selected
// before SDL's own driver list is built, which an environment variable set
// before the test process even starts guarantees; a hint set from inside a
// test would not reliably win if some earlier test already initialized the
// audio subsystem in-process.

void append_bytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const std::size_t offset = out.size();
    out.resize(offset + size);
    std::memcpy(out.data() + offset, data, size);
}
void append_u16(std::vector<std::byte>& out, std::uint16_t value) {
    append_bytes(out, &value, sizeof(value));
}
void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
    append_bytes(out, &value, sizeof(value));
}
void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_bytes(out, &value, sizeof(value));
}
void append_ascii(std::vector<std::byte>& out, std::string_view text) {
    append_bytes(out, text.data(), text.size());
}

// A minimal canonical-format (48kHz, mono, 16-bit PCM) WAV builder,
// deliberately re-implemented here rather than shared with
// tests/atlas-audio/wav_decoder_test.cpp's/decode_cache_test.cpp's own
// versions - matching this project's "duplicating a small pure helper beats
// extracting a shared one after only two callers exist" precedent.
std::vector<std::byte> build_canonical_wav(const std::vector<std::int16_t>& samples) {
    std::vector<std::byte> data_payload;
    for (const std::int16_t sample : samples) {
        append_bytes(data_payload, &sample, sizeof(sample));
    }

    constexpr std::uint16_t channels = 1;
    constexpr std::uint16_t bits_per_sample = 16;
    constexpr std::uint32_t sample_rate = 48000;
    constexpr std::uint32_t block_align = channels * (bits_per_sample / 8);

    std::vector<std::byte> wav;
    append_ascii(wav, "RIFF");
    append_u32(wav, static_cast<std::uint32_t>(4 + 8 + 16 + 8 + data_payload.size()));
    append_ascii(wav, "WAVE");
    append_ascii(wav, "fmt ");
    append_u32(wav, 16);
    append_u16(wav, 1); // PCM
    append_u16(wav, channels);
    append_u32(wav, sample_rate);
    append_u32(wav, sample_rate * block_align);
    append_u16(wav, static_cast<std::uint16_t>(block_align));
    append_u16(wav, bits_per_sample);
    append_ascii(wav, "data");
    append_u32(wav, static_cast<std::uint32_t>(data_payload.size()));
    wav.insert(wav.end(), data_payload.begin(), data_payload.end());
    return wav;
}

std::filesystem::path write_temp_blob(const std::string& name, const std::vector<std::byte>& bytes) {
    const std::filesystem::path path = std::filesystem::path{::testing::TempDir()} / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - ostream::write needs a const char*.
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

// Builds a registry with exactly one playable canonical clip registered
// under a fresh, unique-per-test name, plus a resolvable-but-malformed
// second entry standing in for "a cue that fails to decode".
struct TestFixtureAssets {
    resource::ResourceRegistry registry;
    ResourceId playable_cue;
    ResourceId undecodable_cue;
};

TestFixtureAssets make_fixture_assets(std::string_view test_name) {
    const ResourceId playable =
        ResourceId::from_name(std::string("sfx/sdl3-backend/") + std::string(test_name));
    const ResourceId undecodable =
        ResourceId::from_name(std::string("sfx/sdl3-backend/") + std::string(test_name) + "/broken");

    std::vector<std::byte> blob;
    const std::vector<std::byte> playable_bytes = build_canonical_wav({0, 1000, -1000, 2000, -2000});
    const std::vector<std::byte> garbage_bytes = [] {
        std::vector<std::byte> bytes;
        append_ascii(bytes, "not a wav file at all");
        return bytes;
    }();

    append_u64(blob, 2);
    // Entry headers first (id, offset, size), then the concatenated data section - matching
    // atlas::rcc::pack_resource_blob's documented layout (see decode_cache_test.cpp's own
    // pack_single_entry_blob for the single-entry case this generalizes).
    append_u64(blob, playable.value);
    append_u64(blob, 0);
    append_u64(blob, playable_bytes.size());
    append_u64(blob, undecodable.value);
    append_u64(blob, playable_bytes.size());
    append_u64(blob, garbage_bytes.size());
    blob.insert(blob.end(), playable_bytes.begin(), playable_bytes.end());
    blob.insert(blob.end(), garbage_bytes.begin(), garbage_bytes.end());

    const auto blob_path = write_temp_blob(std::string(test_name) + ".blob", blob);
    return TestFixtureAssets{.registry = resource::ResourceRegistry{{{"Sound", blob_path}}},
                             .playable_cue = playable,
                             .undecodable_cue = undecodable};
}

// Issue #205's own mono-config tests need a clip whose queued audio cannot
// plausibly finish draining between two back-to-back submit() calls in the
// same test - make_fixture_assets()'s own 5-sample clip (already exercised
// by SustainedVoiceLoopsPastItsOwnClipDurationWhileStillSubmitted's
// deliberate SDL_Delay(150) wait) is the wrong shape here: this is a
// same-test-tick race, not a "wait for it to drain" test. ~0.5 seconds
// (24000 samples at 48kHz) is orders of magnitude longer than the
// microseconds between two adjacent function calls, so queued_before is
// reliably nonzero without any real-time wait.
resource::ResourceRegistry make_long_clip_registry(std::string_view test_name, ResourceId cue) {
    std::vector<std::int16_t> samples(24000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<std::int16_t>((i % 2 == 0) ? 1000 : -1000);
    }
    const std::vector<std::byte> playable_bytes = build_canonical_wav(samples);

    std::vector<std::byte> blob;
    append_u64(blob, 1);
    append_u64(blob, cue.value);
    append_u64(blob, 0);
    append_u64(blob, playable_bytes.size());
    blob.insert(blob.end(), playable_bytes.begin(), playable_bytes.end());

    const auto blob_path = write_temp_blob(std::string(test_name) + "-long.blob", blob);
    return resource::ResourceRegistry{{{"Sound", blob_path}}};
}

TEST(Sdl3AudioBackendConstruction, ConstructionSucceedsAgainstTheRealDummyDevice) {
    TestFixtureAssets assets = make_fixture_assets("construction-ok");
    DecodeCache decode_cache{assets.registry, "Sound"};

    EXPECT_NO_THROW({ Sdl3AudioBackend backend{decode_cache}; });
}

TEST(Sdl3AudioBackendConstruction, FailureReportsSdlErrorTextInTheException) {
    // A deterministic construction failure, independent of the sandbox's own
    // hardware - mirrors Sdl3FrameBackendConstruction's/
    // Sdl3RawSignalSourceConstruction's own equivalent test exactly: an
    // invalid SDL_HINT_AUDIO_DRIVER value makes SDL_InitSubSystem(SDL_INIT_AUDIO)
    // itself fail.
    //
    // SDL_SetHint() alone is NOT enough here (confirmed by reading
    // SDL_hints.c's own SDL_SetHintWithPriority: "if (env && (priority <
    // SDL_HINT_OVERRIDE)) return true" - a hint already set from an
    // environment variable at process start silently wins over any later
    // SDL_SetHint() call using a lower priority). This whole test binary
    // runs with SDL_AUDIODRIVER=dummy set as a real environment variable
    // (tests/atlas-audio/CMakeLists.txt, so headless CI always gets a real
    // device) - so overriding it for this one test requires
    // SDL_HINT_OVERRIDE priority explicitly, not the SDL_SetHint() default.
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "this-driver-does-not-exist", SDL_HINT_OVERRIDE);
    TestFixtureAssets assets = make_fixture_assets("construction-fail");
    DecodeCache decode_cache{assets.registry, "Sound"};

    try {
        Sdl3AudioBackend backend{decode_cache};
        FAIL() << "expected SDL_InitSubSystem(SDL_INIT_AUDIO) to fail for an invalid audio driver hint";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string_view{error.what()}.find("SDL"), std::string_view::npos);
    }

    // Restore the real dummy driver for every subsequent test in this binary.
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
}

TEST(Sdl3AudioBackendConstruction, ConstructionWithAMonoConfigSucceeds) {
    TestFixtureAssets assets = make_fixture_assets("construction-mono");
    DecodeCache decode_cache{assets.registry, "Sound"};

    const Sdl3AudioBackendConfig config{.channels = ChannelMode::Mono};
    EXPECT_NO_THROW({ Sdl3AudioBackend backend(decode_cache, config); });
}

class Sdl3AudioBackendTest : public ::testing::Test {
protected:
    void SetUp() override { SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"); }
};

TEST_F(Sdl3AudioBackendTest, SubmitWithANewResolvedCueStartsAVoice) {
    TestFixtureAssets assets = make_fixture_assets("submit-starts-voice");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 1, .generation = 0};

    backend.submit(std::vector<ResolvedCue>{ResolvedCue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.5F, .effective_pan = 0.0F}});

    EXPECT_EQ(backend.active_voice_count(), 1U);
    EXPECT_NE(backend.debug_voice_stream(source), nullptr);
}

TEST_F(Sdl3AudioBackendTest, SubmitOmittingAPreviouslyPresentSourceStopsItsVoice) {
    TestFixtureAssets assets = make_fixture_assets("submit-stops-voice");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 2, .generation = 0};

    backend.submit(std::vector<ResolvedCue>{ResolvedCue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.5F, .effective_pan = 0.0F}});
    ASSERT_EQ(backend.active_voice_count(), 1U);

    backend.submit({});

    EXPECT_EQ(backend.active_voice_count(), 0U);
    EXPECT_EQ(backend.debug_voice_stream(source), nullptr);
}

TEST_F(Sdl3AudioBackendTest, SubmitTwiceWithTheSameSourceUpdatesRatherThanRecreatesTheVoice) {
    TestFixtureAssets assets = make_fixture_assets("submit-updates-voice");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 3, .generation = 0};

    backend.submit(std::vector<ResolvedCue>{ResolvedCue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.2F, .effective_pan = -1.0F}});
    ASSERT_EQ(backend.active_voice_count(), 1U);
    SDL_AudioStream* const first_stream = backend.debug_voice_stream(source);
    ASSERT_NE(first_stream, nullptr);

    // Second submit: same source, different gain AND pan - must update the
    // existing stream in place (same pointer), never create a second one.
    backend.submit(std::vector<ResolvedCue>{ResolvedCue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.9F, .effective_pan = 1.0F}});

    EXPECT_EQ(backend.active_voice_count(), 1U);
    EXPECT_EQ(backend.debug_voice_stream(source), first_stream);
}

TEST_F(Sdl3AudioBackendTest, TriggerStartsAnIndependentOneShotVoiceUntouchedBySubmit) {
    TestFixtureAssets assets = make_fixture_assets("trigger-independent");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 4, .generation = 0};

    backend.trigger(TriggeredCue{.source = source, .cue = assets.playable_cue, .gain = 0.7F, .pan = 0.3F});

    EXPECT_EQ(backend.active_one_shot_count(), 1U);
    // A trigger() never registers a sustained voice for its source, and a
    // subsequent submit() that never mentions this source must not disturb
    // the one-shot at all.
    EXPECT_EQ(backend.active_voice_count(), 0U);

    backend.submit({});

    EXPECT_EQ(backend.active_voice_count(), 0U);
}

// A sustained voice (submit()) must keep sounding for as long as its source
// keeps being submitted, even past its own clip's natural duration - the
// real, motivating use case being ambiance (a looping wind/room-tone bed) or
// a channeled effect's hum (ResolvedCue's own doc comment,
// sound_renderer.hpp), neither of which is expected to fall silent partway
// through a channel/zone just because the underlying clip is short. This
// test's fixture clip is 5 samples (~0.1ms at 48kHz) - far shorter than the
// SDL3 dummy driver's own ~21ms (1024-sample-frame) device buffer pacing
// (src/audio/dummy/SDL_dummyaudio.c, verified against the real fetched
// source), so sleeping past that pacing window guarantees the device has
// actually drained the initially-queued data before the second submit()
// call below, proving the observed non-zero queue afterward is a real
// loop-refill and not merely data that never finished draining in the first
// place.
TEST_F(Sdl3AudioBackendTest, SustainedVoiceLoopsPastItsOwnClipDurationWhileStillSubmitted) {
    TestFixtureAssets assets = make_fixture_assets("submit-loops-voice");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 9, .generation = 0};
    const ResolvedCue cue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.5F, .effective_pan = 0.0F};

    backend.submit(std::vector<ResolvedCue>{cue});
    SDL_AudioStream* const stream = backend.debug_voice_stream(source);
    ASSERT_NE(stream, nullptr);

    // Give the dummy device's own real-time-paced audio thread enough
    // wall-clock time to fully consume this tiny clip's queued data.
    SDL_Delay(150);
    ASSERT_LE(SDL_GetAudioStreamQueued(stream), 0)
        << "test assumption violated: the dummy device did not drain the initially-queued clip in time";

    // The next tick's submit() still names this same source - a real
    // sustained/ambiance voice must therefore be looped (re-queued), not
    // left silent.
    backend.submit(std::vector<ResolvedCue>{cue});

    EXPECT_GT(SDL_GetAudioStreamQueued(stream), 0);
    // Still the same voice, updated in place - not recreated - even though
    // it looped.
    EXPECT_EQ(backend.debug_voice_stream(source), stream);
}

// Issue #205: proves ChannelMode::Mono genuinely takes a different,
// structural code path - not merely accepting a config value cosmetically.
// A Stereo voice's queued bytes are always exactly double a Mono voice's for
// the identical clip (interleaved L/R int16 samples vs. the clip's own
// samples passed through unchanged) - this holds regardless of gain/pan,
// since Mono ignores pan entirely and gain is applied via
// SDL_SetAudioStreamGain (a post-queue scale, never changing how many bytes
// are queued).
TEST_F(Sdl3AudioBackendTest, MonoConfigQueuesHalfTheByteLengthStereoDoesForTheIdenticalClip) {
    const ResourceId cue_id = ResourceId::from_name("sfx/sdl3-backend/mono-config-byte-length");
    resource::ResourceRegistry registry = make_long_clip_registry("mono-config-byte-length", cue_id);
    DecodeCache decode_cache{registry, "Sound"};
    Sdl3AudioBackend stereo_backend{decode_cache};
    Sdl3AudioBackend mono_backend{decode_cache, Sdl3AudioBackendConfig{.channels = ChannelMode::Mono}};
    const EntityRef source{.index = 9, .generation = 0};
    const ResolvedCue cue{.source = source, .cue = cue_id, .effective_gain = 1.0F, .effective_pan = 0.0F};

    stereo_backend.submit(std::vector<ResolvedCue>{cue});
    mono_backend.submit(std::vector<ResolvedCue>{cue});

    SDL_AudioStream* const stereo_stream = stereo_backend.debug_voice_stream(source);
    SDL_AudioStream* const mono_stream = mono_backend.debug_voice_stream(source);
    ASSERT_NE(stereo_stream, nullptr);
    ASSERT_NE(mono_stream, nullptr);

    // Read back-to-back, immediately after submit() - the 24000-sample clip
    // (make_long_clip_registry's own doc comment) is orders of magnitude
    // longer than the dummy driver's own device-buffer pacing, so neither
    // stream has drained any meaningful fraction of it yet.
    const int stereo_queued = SDL_GetAudioStreamQueued(stereo_stream);
    const int mono_queued = SDL_GetAudioStreamQueued(mono_stream);
    ASSERT_GT(stereo_queued, 0);
    ASSERT_GT(mono_queued, 0);
    EXPECT_EQ(stereo_queued, mono_queued * 2);
}

// Issue #205: a changing pan value on an already-playing Mono sustained
// voice must never trigger the Stereo-only "clear and re-queue, restart from
// the beginning" glitch (sdl3_audio_backend.hpp's "Gain vs. pan update
// mechanism") - pan has no audible effect in Mono mode at all, so paying
// that cost would be pure waste. A re-queue always resets the stream's
// queued byte count back up to the clip's full (un-drained) length -
// deliberately waiting for some real draining to occur first (so
// queued_after_wait < queued_before), then asserting the second submit()
// (which only changes pan) never pushes the queued count back UP, is a
// timing-independent way to detect a re-queue: an exact byte-count
// equality check would itself be racy (ordinary consumption between the two
// reads would already change it with no bug present at all), but "never
// increases" holds regardless of exactly how much natural draining occurred
// meanwhile.
TEST_F(Sdl3AudioBackendTest, ChangingPanOnAMonoSustainedVoiceNeverReQueuesIt) {
    const ResourceId cue_id = ResourceId::from_name("sfx/sdl3-backend/mono-config-pan-noop");
    resource::ResourceRegistry registry = make_long_clip_registry("mono-config-pan-noop", cue_id);
    DecodeCache decode_cache{registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache, Sdl3AudioBackendConfig{.channels = ChannelMode::Mono}};
    const EntityRef source{.index = 10, .generation = 0};

    backend.submit(std::vector<ResolvedCue>{
        ResolvedCue{.source = source, .cue = cue_id, .effective_gain = 1.0F, .effective_pan = -1.0F}});
    SDL_AudioStream* const stream = backend.debug_voice_stream(source);
    ASSERT_NE(stream, nullptr);
    const int queued_before = SDL_GetAudioStreamQueued(stream);
    ASSERT_GT(queued_before, 0);

    // Give the dummy device's own real-time-paced audio thread a chance to
    // consume at least one buffer's worth (~21ms at 48kHz/1024 frames) -
    // this clip (0.5s) is nowhere near draining, but this guarantees
    // queued_after_wait is strictly less than queued_before, so the
    // assertion below is meaningful rather than a coincidence of zero
    // elapsed time.
    SDL_Delay(50);
    const int queued_after_wait = SDL_GetAudioStreamQueued(stream);
    ASSERT_LT(queued_after_wait, queued_before);
    ASSERT_GT(queued_after_wait, 0);

    // Same source, same cue, gain unchanged, pan flipped to full right - in
    // Stereo mode this would clear and re-queue; in Mono mode it must be a
    // pure no-op against the stream itself.
    backend.submit(std::vector<ResolvedCue>{
        ResolvedCue{.source = source, .cue = cue_id, .effective_gain = 1.0F, .effective_pan = 1.0F}});

    EXPECT_EQ(backend.debug_voice_stream(source), stream);
    EXPECT_LE(SDL_GetAudioStreamQueued(stream), queued_after_wait);
}

TEST_F(Sdl3AudioBackendTest, ResolvedCueReferencingAnUndecodableResourceProducesNoVoice) {
    TestFixtureAssets assets = make_fixture_assets("submit-undecodable");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 5, .generation = 0};

    EXPECT_NO_THROW({
        backend.submit(std::vector<ResolvedCue>{ResolvedCue{
            .source = source, .cue = assets.undecodable_cue, .effective_gain = 0.5F, .effective_pan = 0.0F}});
    });

    EXPECT_EQ(backend.active_voice_count(), 0U);
    EXPECT_EQ(backend.debug_voice_stream(source), nullptr);
}

TEST_F(Sdl3AudioBackendTest, TriggeredCueReferencingAnUndecodableResourceProducesNoVoice) {
    TestFixtureAssets assets = make_fixture_assets("trigger-undecodable");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend backend{decode_cache};
    const EntityRef source{.index = 6, .generation = 0};

    EXPECT_NO_THROW({
        backend.trigger(
            TriggeredCue{.source = source, .cue = assets.undecodable_cue, .gain = 0.5F, .pan = 0.0F});
    });

    EXPECT_EQ(backend.active_one_shot_count(), 0U);
}

TEST_F(Sdl3AudioBackendTest, MoveConstructionTransfersOwnershipAndLeavesInstanceUsable) {
    TestFixtureAssets assets = make_fixture_assets("move-construct");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend original{decode_cache};
    const EntityRef source{.index = 7, .generation = 0};
    original.submit(std::vector<ResolvedCue>{ResolvedCue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.5F, .effective_pan = 0.0F}});
    ASSERT_EQ(original.active_voice_count(), 1U);

    Sdl3AudioBackend moved(std::move(original));

    EXPECT_EQ(moved.active_voice_count(), 1U);
    EXPECT_NO_FATAL_FAILURE({ moved.submit({}); });
    EXPECT_EQ(moved.active_voice_count(), 0U);
}

TEST_F(Sdl3AudioBackendTest, MoveAssignmentTransfersOwnershipAndLeavesInstanceUsable) {
    TestFixtureAssets assets = make_fixture_assets("move-assign");
    DecodeCache decode_cache{assets.registry, "Sound"};
    Sdl3AudioBackend first{decode_cache};
    Sdl3AudioBackend second{decode_cache};
    const EntityRef source{.index = 8, .generation = 0};
    first.submit(std::vector<ResolvedCue>{ResolvedCue{
        .source = source, .cue = assets.playable_cue, .effective_gain = 0.5F, .effective_pan = 0.0F}});
    ASSERT_EQ(first.active_voice_count(), 1U);

    second = std::move(first);

    EXPECT_EQ(second.active_voice_count(), 1U);
    EXPECT_NO_FATAL_FAILURE({
        static_cast<void>(second.trigger(
            TriggeredCue{.source = source, .cue = assets.playable_cue, .gain = 0.4F, .pan = 0.0F}));
    });
}

} // namespace
} // namespace atlas::audio
