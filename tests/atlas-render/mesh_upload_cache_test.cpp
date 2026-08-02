#include "atlas/render/mesh_upload_cache.hpp"

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
// - real fixture files on disk (triangle.mesh, mesh_asset_test.cpp's own
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

// Hand-packs the single-entry resource blob format
// atlas::rcc::pack_resource_blob documents, mirroring
// tests/atlas-audio/decode_cache_test.cpp's own pack_single_entry_blob
// exactly (this cache's own direct template, per issue #154) - independently
// re-implemented here rather than shared, the same reasoning that file's own
// comment gives for not linking atlas-rcc into a library's test binary.
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

// Mirrors Sdl3FrameBackendTest's own headless-CI pattern
// (sdl3_frame_backend_test.cpp, issue #151's README decision): attempts real
// SDL3/SDL_GPU window+device creation in SetUp(), SDL_HINT_VIDEO_DRIVER
// forced to "dummy" so windowing itself always succeeds headlessly, and
// GTEST_SKIP()s the moment device creation fails (the expected outcome on
// this project's own sandboxed dev environment / most CI runners - no
// /dev/dri, no Vulkan ICD).
class MeshUploadCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError();
        }

        window_ = SDL_CreateWindow("atlas-render-mesh-upload-cache-tests", 64, 64, SDL_WINDOW_HIDDEN);
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

TEST_F(MeshUploadCacheTest, FirstRequestResolvesDecodesAndUploadsSuccessfully) {
    const ResourceId id = ResourceId::from_name("meshes/mesh-upload-cache/triangle");
    const auto blob_path = write_temp_blob("mesh_upload_cache_ok.blob",
                                           pack_single_entry_blob(id, read_fixture("triangle.mesh")));
    const resource::ResourceRegistry registry{{{"Mesh", blob_path}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    const MeshUploadResult& result = cache.get_or_upload(id);

    ASSERT_EQ(result.status, MeshUploadCacheStatus::Ok);
    EXPECT_NE(result.vertex_buffer, nullptr);
    EXPECT_NE(result.index_buffer, nullptr);
    EXPECT_EQ(result.index_count, 3U);

    cache.release();
}

TEST_F(MeshUploadCacheTest, RepeatedRequestsForTheSameIdReturnTheIdenticalCachedBuffersWithoutReuploading) {
    const ResourceId id = ResourceId::from_name("meshes/mesh-upload-cache/repeated");
    const auto blob_path = write_temp_blob("mesh_upload_cache_repeat.blob",
                                           pack_single_entry_blob(id, read_fixture("triangle.mesh")));
    const resource::ResourceRegistry registry{{{"Mesh", blob_path}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    const MeshUploadResult& first = cache.get_or_upload(id);
    SDL_GPUBuffer* first_vertex_buffer = first.vertex_buffer;
    const MeshUploadResult& second = cache.get_or_upload(id);

    // A cache hit returns the exact same previously-uploaded buffer handle -
    // a fresh upload would allocate a brand new SDL_GPUBuffer. This is the
    // strongest signal available without instrumenting/mocking SDL_GPU,
    // which this project's "test the real thing" convention avoids doing
    // (mirroring DecodeCache's own equivalent test, decode_cache_test.cpp).
    EXPECT_EQ(second.vertex_buffer, first_vertex_buffer);
    EXPECT_EQ(&first, &second);

    cache.release();
}

TEST_F(MeshUploadCacheTest, UnresolvedIdReturnsUnresolvedStatus) {
    const auto blob_path =
        write_temp_blob("mesh_upload_cache_unresolved.blob",
                        pack_single_entry_blob(ResourceId::from_name("meshes/mesh-upload-cache/other"),
                                               read_fixture("triangle.mesh")));
    const resource::ResourceRegistry registry{{{"Mesh", blob_path}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    const MeshUploadResult& result =
        cache.get_or_upload(ResourceId::from_name("meshes/mesh-upload-cache/missing"));

    EXPECT_EQ(result.status, MeshUploadCacheStatus::Unresolved);
    EXPECT_EQ(result.vertex_buffer, nullptr);

    cache.release();
}

TEST_F(MeshUploadCacheTest, ResolutionFailedWhenTheRegisteredBlobFailsToLoad) {
    const resource::ResourceRegistry registry{
        {{"Mesh", std::filesystem::path{::testing::TempDir()} / "does-not-exist.blob"}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    const MeshUploadResult& result =
        cache.get_or_upload(ResourceId::from_name("meshes/mesh-upload-cache/anything"));

    EXPECT_EQ(result.status, MeshUploadCacheStatus::ResolutionFailed);

    cache.release();
}

TEST_F(MeshUploadCacheTest, MalformedBytesReturnDecodeFailedStatus) {
    const ResourceId id = ResourceId::from_name("meshes/mesh-upload-cache/garbage");
    const auto blob_path = write_temp_blob("mesh_upload_cache_malformed.blob",
                                           pack_single_entry_blob(id, read_fixture("truncated_header.mesh")));
    const resource::ResourceRegistry registry{{{"Mesh", blob_path}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    const MeshUploadResult& result = cache.get_or_upload(id);

    EXPECT_EQ(result.status, MeshUploadCacheStatus::DecodeFailed);
    EXPECT_EQ(result.vertex_buffer, nullptr);

    cache.release();
}

TEST_F(MeshUploadCacheTest, RepeatedRequestsForAPermanentFailureReturnTheSameStatusEveryTime) {
    const ResourceId id = ResourceId::from_name("meshes/mesh-upload-cache/permanently-broken");
    const auto blob_path = write_temp_blob("mesh_upload_cache_permanent_failure.blob",
                                           pack_single_entry_blob(id, read_fixture("truncated_header.mesh")));
    const resource::ResourceRegistry registry{{{"Mesh", blob_path}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    const MeshUploadResult& first = cache.get_or_upload(id);
    const MeshUploadResult& second = cache.get_or_upload(id);

    EXPECT_EQ(first.status, MeshUploadCacheStatus::DecodeFailed);
    EXPECT_EQ(second.status, MeshUploadCacheStatus::DecodeFailed);

    cache.release();
}

TEST_F(MeshUploadCacheTest, ReleaseIsIdempotentAndSafeToCallTwice) {
    const ResourceId id = ResourceId::from_name("meshes/mesh-upload-cache/release-twice");
    const auto blob_path = write_temp_blob("mesh_upload_cache_release_twice.blob",
                                           pack_single_entry_blob(id, read_fixture("triangle.mesh")));
    const resource::ResourceRegistry registry{{{"Mesh", blob_path}}};
    MeshUploadCache cache{registry, "Mesh", device()};

    ASSERT_EQ(cache.get_or_upload(id).status, MeshUploadCacheStatus::Ok);

    cache.release();
    // Reaching this line without a double-free crash (ASan/UBSan enabled in
    // the debug preset) is the assertion - the second call must be a no-op,
    // not a double release, mirroring destroy_sdl3_triangle_pipeline's own
    // idempotency precedent.
    EXPECT_NO_THROW(cache.release());
}

} // namespace
} // namespace atlas::render
