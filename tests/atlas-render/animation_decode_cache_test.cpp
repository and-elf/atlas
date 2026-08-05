#include "atlas/render/animation_decode_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

namespace atlas::render {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/
// - real fixture files on disk, mirroring animation_asset_test.cpp's own
// read_fixture helper exactly.
constexpr std::string_view fixtures_dir = ATLAS_RENDER_TEST_FIXTURES_DIR;

std::vector<std::byte> read_fixture(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path{fixtures_dir} / name;
    std::ifstream file(path, std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<std::byte> bytes(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        bytes[i] = static_cast<std::byte>(raw[i]);
    }
    return bytes;
}

void append_bytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const std::size_t offset = out.size();
    out.resize(offset + size);
    std::memcpy(out.data() + offset, data, size);
}

void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_bytes(out, &value, sizeof(value));
}

// Hand-packs the single-entry resource blob format
// atlas::rcc::pack_resource_blob documents, mirroring
// tests/atlas-audio/decode_cache_test.cpp's own pack_single_entry_blob and
// tests/atlas-render/mesh_upload_cache_test.cpp/skeleton tests' own
// independent re-implementations exactly - the same reasoning those files'
// own comments give for not linking atlas-rcc into a library's test binary.
std::vector<std::byte> pack_single_entry_blob(ResourceId id, const std::vector<std::byte>& data) {
    std::vector<std::byte> blob;
    append_u64(blob, 1);
    append_u64(blob, id.value);
    append_u64(blob, 0);
    append_u64(blob, data.size());
    blob.insert(blob.end(), data.begin(), data.end());
    return blob;
}

std::filesystem::path write_temp_blob(const std::string& name, const std::vector<std::byte>& bytes) {
    const std::filesystem::path path = std::filesystem::path{::testing::TempDir()} / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - ostream::write needs a const char*.
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

TEST(AnimationDecodeCache, FirstRequestResolvesAndDecodesSuccessfully) {
    const ResourceId id = ResourceId::from_name("animations/animation-decode-cache/canonical");
    const auto blob_path =
        write_temp_blob("animation_decode_cache_ok.blob",
                        pack_single_entry_blob(id, read_fixture("two_joint_two_keyframe.animation")));
    const resource::ResourceRegistry registry{{{"Animation", blob_path}}};
    AnimationDecodeCache cache{registry, "Animation"};

    const AnimationDecodeResult& result = cache.get_or_decode(id);

    ASSERT_EQ(result.status, AnimationDecodeCacheStatus::Ok);
    EXPECT_EQ(result.animation.joint_count, 2U);
    EXPECT_EQ(result.animation.keyframes.size(), 2U);
}

TEST(AnimationDecodeCache, RepeatedRequestsForTheSameIdReturnTheIdenticalCachedAnimationWithoutRedecoding) {
    const ResourceId id = ResourceId::from_name("animations/animation-decode-cache/repeated");
    const auto blob_path =
        write_temp_blob("animation_decode_cache_repeat.blob",
                        pack_single_entry_blob(id, read_fixture("two_joint_two_keyframe.animation")));
    const resource::ResourceRegistry registry{{{"Animation", blob_path}}};
    AnimationDecodeCache cache{registry, "Animation"};

    const AnimationDecodeResult& first = cache.get_or_decode(id);
    const AnimationKeyframe* first_keyframes_data = first.animation.keyframes.data();
    const AnimationDecodeResult& second = cache.get_or_decode(id);

    // A cache hit returns the exact same previously-decoded DecodedAnimation
    // - a fresh decode would allocate a brand new keyframes vector with a
    // different backing buffer. This is the strongest signal available
    // without instrumenting/mocking ResourceRegistry, mirroring
    // atlas::audio::DecodeCache's own equivalent test.
    EXPECT_EQ(second.animation.keyframes.data(), first_keyframes_data);
    EXPECT_EQ(&first, &second);
}

TEST(AnimationDecodeCache, UnresolvedIdReturnsUnresolvedStatus) {
    const auto blob_path = write_temp_blob(
        "animation_decode_cache_unresolved.blob",
        pack_single_entry_blob(ResourceId::from_name("animations/animation-decode-cache/other"),
                               read_fixture("two_joint_two_keyframe.animation")));
    const resource::ResourceRegistry registry{{{"Animation", blob_path}}};
    AnimationDecodeCache cache{registry, "Animation"};

    const AnimationDecodeResult& result =
        cache.get_or_decode(ResourceId::from_name("animations/animation-decode-cache/missing"));

    EXPECT_EQ(result.status, AnimationDecodeCacheStatus::Unresolved);
}

TEST(AnimationDecodeCache, ResolutionFailedWhenTheRegisteredBlobFailsToLoad) {
    const resource::ResourceRegistry registry{
        {{"Animation", std::filesystem::path{::testing::TempDir()} / "does-not-exist.blob"}}};
    AnimationDecodeCache cache{registry, "Animation"};

    const AnimationDecodeResult& result =
        cache.get_or_decode(ResourceId::from_name("animations/animation-decode-cache/anything"));

    EXPECT_EQ(result.status, AnimationDecodeCacheStatus::ResolutionFailed);
}

TEST(AnimationDecodeCache, MalformedBytesReturnDecodeFailedStatus) {
    const ResourceId id = ResourceId::from_name("animations/animation-decode-cache/garbage");
    const auto blob_path =
        write_temp_blob("animation_decode_cache_malformed.blob",
                        pack_single_entry_blob(id, read_fixture("truncated_header.animation")));
    const resource::ResourceRegistry registry{{{"Animation", blob_path}}};
    AnimationDecodeCache cache{registry, "Animation"};

    const AnimationDecodeResult& result = cache.get_or_decode(id);

    EXPECT_EQ(result.status, AnimationDecodeCacheStatus::DecodeFailed);
}

TEST(AnimationDecodeCache, RepeatedRequestsForAPermanentFailureReturnTheSameStatusEveryTime) {
    const ResourceId id = ResourceId::from_name("animations/animation-decode-cache/permanently-broken");
    const auto blob_path =
        write_temp_blob("animation_decode_cache_permanent_failure.blob",
                        pack_single_entry_blob(id, read_fixture("truncated_header.animation")));
    const resource::ResourceRegistry registry{{{"Animation", blob_path}}};
    AnimationDecodeCache cache{registry, "Animation"};

    const AnimationDecodeResult& first = cache.get_or_decode(id);
    const AnimationDecodeResult& second = cache.get_or_decode(id);

    EXPECT_EQ(first.status, AnimationDecodeCacheStatus::DecodeFailed);
    EXPECT_EQ(second.status, AnimationDecodeCacheStatus::DecodeFailed);
}

} // namespace
} // namespace atlas::render
