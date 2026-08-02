#include "atlas/render/texture_upload_cache.hpp"

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::render {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/
// - real fixture files on disk (checker.tex, texture_asset_test.cpp's own
// well-formed fixture), not mocked bytes.
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

// Mirrors mesh_upload_cache_test.cpp's own pack_single_entry_blob exactly
// (itself mirroring tests/atlas-audio/decode_cache_test.cpp) - duplicated
// rather than shared, matching this project's own small-helper-duplication
// precedent (e.g. atlas::ResourceId's FNV-1a comment).
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

// Mirrors MeshUploadCacheTest's own headless-CI pattern (mesh_upload_cache_test.cpp),
// including its "offscreen" video-driver choice (issue #155).
class TextureUploadCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError();
        }

        window_ = SDL_CreateWindow("atlas-render-texture-upload-cache-tests", 64, 64, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            const std::string error = SDL_GetError();
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateWindow failed: " << error;
        }

        constexpr SDL_GPUShaderFormat supported_shader_formats =
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;
        device_ = SDL_CreateGPUDevice(supported_shader_formats, /*debug_mode=*/false, /*name=*/nullptr);
        if (device_ == nullptr) {
            const std::string error = SDL_GetError();
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                            "(expected on most headless CI runners - see "
                            "libraries/atlas-render/README.md's headless-CI decision): "
                         << error;
        }
    }

    void TearDown() override {
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        if (window_ != nullptr || device_ != nullptr) {
            SDL_Quit();
        }
    }

    SDL_GPUDevice* device() { return device_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
};

TEST_F(TextureUploadCacheTest, FirstRequestResolvesDecodesAndUploadsSuccessfully) {
    const ResourceId id = ResourceId::from_name("textures/texture-upload-cache/checker");
    const auto blob_path = write_temp_blob("texture_upload_cache_ok.blob",
                                           pack_single_entry_blob(id, read_fixture("checker.tex")));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& result = cache.get_or_upload(id);

    ASSERT_EQ(result.status, TextureUploadCacheStatus::Ok);
    EXPECT_NE(result.texture, nullptr);
    EXPECT_EQ(result.width, 2U);
    EXPECT_EQ(result.height, 2U);

    cache.release();
}

TEST_F(TextureUploadCacheTest,
       RepeatedRequestsForTheSameIdReturnTheIdenticalCachedTextureWithoutReuploading) {
    const ResourceId id = ResourceId::from_name("textures/texture-upload-cache/repeated");
    const auto blob_path = write_temp_blob("texture_upload_cache_repeat.blob",
                                           pack_single_entry_blob(id, read_fixture("checker.tex")));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& first = cache.get_or_upload(id);
    SDL_GPUTexture* first_texture = first.texture;
    const TextureUploadResult& second = cache.get_or_upload(id);

    EXPECT_EQ(second.texture, first_texture);
    EXPECT_EQ(&first, &second);

    cache.release();
}

TEST_F(TextureUploadCacheTest, UnresolvedIdReturnsUnresolvedStatus) {
    const auto blob_path =
        write_temp_blob("texture_upload_cache_unresolved.blob",
                        pack_single_entry_blob(ResourceId::from_name("textures/texture-upload-cache/other"),
                                               read_fixture("checker.tex")));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& result =
        cache.get_or_upload(ResourceId::from_name("textures/texture-upload-cache/missing"));

    EXPECT_EQ(result.status, TextureUploadCacheStatus::Unresolved);
    EXPECT_EQ(result.texture, nullptr);

    cache.release();
}

TEST_F(TextureUploadCacheTest, ResolutionFailedWhenTheRegisteredBlobFailsToLoad) {
    const resource::ResourceRegistry registry{
        {{"Texture", std::filesystem::path{::testing::TempDir()} / "does-not-exist.blob"}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& result =
        cache.get_or_upload(ResourceId::from_name("textures/texture-upload-cache/anything"));

    EXPECT_EQ(result.status, TextureUploadCacheStatus::ResolutionFailed);

    cache.release();
}

TEST_F(TextureUploadCacheTest, MalformedBytesReturnDecodeFailedStatus) {
    const ResourceId id = ResourceId::from_name("textures/texture-upload-cache/garbage");
    const auto blob_path = write_temp_blob("texture_upload_cache_malformed.blob",
                                           pack_single_entry_blob(id, read_fixture("truncated_header.tex")));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& result = cache.get_or_upload(id);

    EXPECT_EQ(result.status, TextureUploadCacheStatus::DecodeFailed);
    EXPECT_EQ(result.texture, nullptr);

    cache.release();
}

TEST_F(TextureUploadCacheTest, RepeatedRequestsForAPermanentFailureReturnTheSameStatusEveryTime) {
    const ResourceId id = ResourceId::from_name("textures/texture-upload-cache/permanently-broken");
    const auto blob_path = write_temp_blob("texture_upload_cache_permanent_failure.blob",
                                           pack_single_entry_blob(id, read_fixture("truncated_header.tex")));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& first = cache.get_or_upload(id);
    const TextureUploadResult& second = cache.get_or_upload(id);

    EXPECT_EQ(first.status, TextureUploadCacheStatus::DecodeFailed);
    EXPECT_EQ(second.status, TextureUploadCacheStatus::DecodeFailed);

    cache.release();
}

TEST_F(TextureUploadCacheTest, ZeroDimensionTextureIsOkWithANullTextureHandle) {
    // Header-only bytes declaring zero width/height is well-formed per
    // decode_texture's own format (texture_asset.hpp) - an empty texture,
    // not a malformed one; this cache has nothing to upload for it, but
    // that is not itself a failure (see TextureUploadResult's own doc
    // comment).
    const ResourceId id = ResourceId::from_name("textures/texture-upload-cache/empty");
    const std::vector<std::byte> zero_dimension_bytes(8, std::byte{0});
    const auto blob_path =
        write_temp_blob("texture_upload_cache_empty.blob", pack_single_entry_blob(id, zero_dimension_bytes));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    const TextureUploadResult& result = cache.get_or_upload(id);

    EXPECT_EQ(result.status, TextureUploadCacheStatus::Ok);
    EXPECT_EQ(result.texture, nullptr);
    EXPECT_EQ(result.width, 0U);
    EXPECT_EQ(result.height, 0U);

    cache.release();
}

TEST_F(TextureUploadCacheTest, ReleaseIsIdempotentAndSafeToCallTwice) {
    const ResourceId id = ResourceId::from_name("textures/texture-upload-cache/release-twice");
    const auto blob_path = write_temp_blob("texture_upload_cache_release_twice.blob",
                                           pack_single_entry_blob(id, read_fixture("checker.tex")));
    const resource::ResourceRegistry registry{{{"Texture", blob_path}}};
    TextureUploadCache cache{registry, "Texture", device()};

    ASSERT_EQ(cache.get_or_upload(id).status, TextureUploadCacheStatus::Ok);

    cache.release();
    EXPECT_NO_THROW(cache.release());
}

} // namespace
} // namespace atlas::render
