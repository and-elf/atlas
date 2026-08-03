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
