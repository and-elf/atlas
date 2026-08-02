#include "atlas/audio/decode_cache.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace atlas::audio {
namespace {

void append_bytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::byte*>(data);
    out.insert(out.end(), bytes, bytes + size);
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

// A minimal WAV builder, deliberately re-implemented here rather than
// shared with tests/atlas-audio/wav_decoder_test.cpp's own richer version -
// this file only ever needs exactly one canonical clip and one
// wrong-format clip, matching this project's "duplicating a small pure
// helper beats extracting a shared one after only two callers exist"
// precedent (e.g. atlas::ResourceId's own comment on FNV-1a).
std::vector<std::byte> build_wav(std::uint16_t channels, const std::vector<std::int16_t>& samples) {
    std::vector<std::byte> data_payload;
    for (const std::int16_t sample : samples) {
        append_bytes(data_payload, &sample, sizeof(sample));
    }

    constexpr std::uint16_t bits_per_sample = 16;
    const std::uint32_t sample_rate = 48000;
    const std::uint32_t block_align = static_cast<std::uint32_t>(channels) * (bits_per_sample / 8);

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

std::vector<std::byte> canonical_wav_bytes() {
    return build_wav(1, {1, 2, 3, 4});
}
std::vector<std::byte> stereo_wav_bytes() {
    return build_wav(2, {1, 2, 3, 4});
}

std::vector<std::byte> garbage_bytes() {
    std::vector<std::byte> bytes;
    append_ascii(bytes, "not a wav file at all");
    return bytes;
}

// Hand-packs the single-entry resource blob format
// atlas::rcc::pack_resource_blob documents (tools/atlas-rcc/include/atlas/rcc/resource_blob.hpp):
// u64 entry_count, then entry_count x {u64 id, u64 offset, u64 size}, then the
// data section - independently re-implemented rather than linking atlas-rcc
// itself into this test binary, the same reasoning
// atlas::resource::ResourceRegistry's own header already documents for why it
// doesn't depend on atlas-rcc (a tool depending on a library's tests would be
// a backwards, tool-into-library-test edge this project's dependency
// direction doesn't otherwise have).
std::vector<std::byte> pack_single_entry_blob(ResourceId id, const std::vector<std::byte>& data) {
    std::vector<std::byte> blob;
    append_u64(blob, 1);
    append_u64(blob, id.value);
    append_u64(blob, 0);
    append_u64(blob, data.size());
    blob.insert(blob.end(), data.begin(), data.end());
    return blob;
}

// Writes `bytes` to a fresh file under GoogleTest's own per-test-run temp
// directory and returns its path - a real file on disk, not a mocked
// filesystem, matching CLAUDE.md's "test via a minimal test host, not by
// mocking behavior" rule the same way tests/atlas-resource/fixtures/'s
// checked-in blobs do, just generated at test time instead of checked in,
// since this file's blobs are trivial enough to build inline.
std::filesystem::path write_temp_blob(const std::string& name, const std::vector<std::byte>& bytes) {
    const std::filesystem::path path = std::filesystem::path{::testing::TempDir()} / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - ostream::write needs a const char*.
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

TEST(DecodeCache, FirstRequestResolvesAndDecodesSuccessfully) {
    const ResourceId id = ResourceId::from_name("sfx/decode-cache/canonical");
    const auto blob_path =
        write_temp_blob("decode_cache_ok.blob", pack_single_entry_blob(id, canonical_wav_bytes()));
    const resource::ResourceRegistry registry{{{"Sound", blob_path}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& result = cache.get_or_decode(id);

    ASSERT_EQ(result.status, DecodeCacheStatus::Ok);
    EXPECT_EQ(result.clip.samples, (std::vector<std::int16_t>{1, 2, 3, 4}));
}

TEST(DecodeCache, RepeatedRequestsForTheSameIdReturnTheIdenticalCachedClipWithoutRedecoding) {
    const ResourceId id = ResourceId::from_name("sfx/decode-cache/repeated");
    const auto blob_path =
        write_temp_blob("decode_cache_repeat.blob", pack_single_entry_blob(id, canonical_wav_bytes()));
    const resource::ResourceRegistry registry{{{"Sound", blob_path}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& first = cache.get_or_decode(id);
    const std::int16_t* first_samples_data = first.clip.samples.data();
    const DecodeCacheResult& second = cache.get_or_decode(id);

    // A cache hit returns the exact same previously-decoded DecodedClip - a
    // fresh decode would allocate a brand new samples vector with a
    // different backing buffer. This is the strongest signal available
    // without instrumenting/mocking ResourceRegistry, which this project's
    // "test the real thing" convention avoids doing.
    EXPECT_EQ(second.clip.samples.data(), first_samples_data);
    EXPECT_EQ(&first, &second);
}

TEST(DecodeCache, UnresolvedIdReturnsUnresolvedStatus) {
    const auto blob_path = write_temp_blob(
        "decode_cache_unresolved.blob",
        pack_single_entry_blob(ResourceId::from_name("sfx/decode-cache/other"), canonical_wav_bytes()));
    const resource::ResourceRegistry registry{{{"Sound", blob_path}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& result = cache.get_or_decode(ResourceId::from_name("sfx/decode-cache/missing"));

    EXPECT_EQ(result.status, DecodeCacheStatus::Unresolved);
}

TEST(DecodeCache, ResolutionFailedWhenTheRegisteredBlobFailsToLoad) {
    const resource::ResourceRegistry registry{
        {{"Sound", std::filesystem::path{::testing::TempDir()} / "does-not-exist.blob"}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& result = cache.get_or_decode(ResourceId::from_name("sfx/decode-cache/anything"));

    EXPECT_EQ(result.status, DecodeCacheStatus::ResolutionFailed);
}

TEST(DecodeCache, MalformedBytesReturnDecodeMalformedStatus) {
    const ResourceId id = ResourceId::from_name("sfx/decode-cache/garbage");
    const auto blob_path =
        write_temp_blob("decode_cache_malformed.blob", pack_single_entry_blob(id, garbage_bytes()));
    const resource::ResourceRegistry registry{{{"Sound", blob_path}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& result = cache.get_or_decode(id);

    EXPECT_EQ(result.status, DecodeCacheStatus::DecodeMalformed);
}

TEST(DecodeCache, WrongFormatBytesReturnDecodeUnsupportedFormatStatus) {
    const ResourceId id = ResourceId::from_name("sfx/decode-cache/stereo");
    const auto blob_path =
        write_temp_blob("decode_cache_unsupported.blob", pack_single_entry_blob(id, stereo_wav_bytes()));
    const resource::ResourceRegistry registry{{{"Sound", blob_path}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& result = cache.get_or_decode(id);

    EXPECT_EQ(result.status, DecodeCacheStatus::DecodeUnsupportedFormat);
}

TEST(DecodeCache, RepeatedRequestsForAPermanentFailureReturnTheSameStatusEveryTime) {
    const ResourceId id = ResourceId::from_name("sfx/decode-cache/permanently-broken");
    const auto blob_path =
        write_temp_blob("decode_cache_permanent_failure.blob", pack_single_entry_blob(id, garbage_bytes()));
    const resource::ResourceRegistry registry{{{"Sound", blob_path}}};
    DecodeCache cache{registry, "Sound"};

    const DecodeCacheResult& first = cache.get_or_decode(id);
    const DecodeCacheResult& second = cache.get_or_decode(id);

    EXPECT_EQ(first.status, DecodeCacheStatus::DecodeMalformed);

    EXPECT_EQ(second.status, DecodeCacheStatus::DecodeMalformed);
}

} // namespace
} // namespace atlas::audio
