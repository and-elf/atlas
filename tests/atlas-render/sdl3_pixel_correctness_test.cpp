#include "atlas/render/mesh_asset.hpp"
#include "atlas/render/mesh_upload_cache.hpp"
#include "atlas/render/sdl3_mesh_pipeline.hpp"
#include "atlas/render/texture_upload_cache.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atlas::render {
namespace {

// Issue #155: the actual point of this whole issue - proving Sdl3FrameBackend's
// underlying render path produces *correct pixels*, not merely "executes
// without throwing" (#151-#154's own tests, sdl3_frame_backend_test.cpp,
// already cover the latter). Exercises Sdl3MeshPipeline and both real
// upload caches directly (MeshUploadCache/TextureUploadCache) rather than
// going through Sdl3FrameBackend::submit() - a deliberate choice, not a
// shortcut: submit() only ever renders into the window's own swapchain
// texture, and SDL3's "offscreen" video driver (this file's own
// SDL_HINT_VIDEO_DRIVER choice, matching the rest of this retrofit - see
// sdl3_frame_backend_test.cpp's doc comment) has no real presentable
// surface, so SDL_WaitAndAcquireGPUSwapchainTexture legitimately returns a
// null texture there (verified directly against a standalone probe against
// this project's own fetched SDL3 build before writing this test - the
// same "no presentable image this frame" case a minimized real window
// produces, not an error). Every other piece downstream of that
// window/swapchain seam - real resource resolution, decode, GPU upload,
// and the mesh pipeline's draw call - is exactly the same real production
// code Sdl3FrameBackend::submit() itself calls (mirrors this library's own
// established precedent for isolating the shader/pipeline mechanism from
// window/swapchain bring-up, e.g. the now-deleted
// sdl3_shader_pipeline_test.cpp's DrawInsideARealRenderPassDoesNotThrowOrCrash,
// git history at 93107fb).
//
// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/.
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

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
    append_bytes(out, &value, sizeof(value));
}

void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_bytes(out, &value, sizeof(value));
}

void append_float(std::vector<std::byte>& out, float value) {
    append_bytes(out, &value, sizeof(value));
}

// Hand-packs decode_mesh's own documented binary format (mesh_asset.hpp) for
// a caller-supplied vertex/index list - the same format triangle.mesh (this
// library's checked-in fixture) already follows, generated here rather than
// checked in as its own binary fixture file: this quad exists purely to
// give the pixel-correctness assertions below an exact, known shape, and a
// small procedural helper documenting its own intent in code is clearer
// than hand-encoding raw floats into another opaque binary file (the
// project's own "your call, document your choice" latitude - issue #155).
std::vector<std::byte> pack_decoded_mesh_bytes(const std::vector<Vertex>& vertices,
                                               const std::vector<std::uint32_t>& indices) {
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(vertices.size()));
    append_u32(bytes, static_cast<std::uint32_t>(indices.size()));
    for (const Vertex& vertex : vertices) {
        append_float(bytes, vertex.position.x);
        append_float(bytes, vertex.position.y);
        append_float(bytes, vertex.position.z);
        append_float(bytes, vertex.normal.x);
        append_float(bytes, vertex.normal.y);
        append_float(bytes, vertex.normal.z);
        append_float(bytes, vertex.u);
        append_float(bytes, vertex.v);
    }
    for (const std::uint32_t index : indices) {
        append_u32(bytes, index);
    }
    return bytes;
}

// A single quad, authored directly in NDC/clip space (spec: no Camera/
// view-projection concept exists anywhere in Atlas yet - #154's own locked-
// in scope, transform.hpp's to_model_matrix doc comment) - filling the
// entire render target from corner to corner, exactly like the issue
// describes. mesh.vert.hlsl computes `mul(Model, float4(Position, 1.0))`;
// with the identity Transform this test drives (see TEST_F below), Model is
// the identity matrix, so each vertex's clip-space position is exactly its
// authored Position, and (w == 1) its NDC position too.
//
// SDL_GPU's own documented coordinate system (SDL_gpu.h, "Coordinate
// System"): NDC lower-left is (-1,-1), upper-right is (1,1) - +Y is up;
// texture coordinates' top-left is (0,0) - +V is down. So the screen's
// top-left corner is NDC (-1, 1) with UV (0, 0), and decode_texture's own
// documented pixel layout (texture_asset.hpp: "row-major, top row first")
// puts checker.tex's top-left texel (opaque red, per
// texture_asset_test.cpp) at that same UV origin.
std::vector<Vertex> quad_vertices() {
    return {
        Vertex{.position = {-1.0F, 1.0F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .u = 0.0F, .v = 0.0F},
        Vertex{.position = {1.0F, 1.0F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .u = 1.0F, .v = 0.0F},
        Vertex{.position = {-1.0F, -1.0F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .u = 0.0F, .v = 1.0F},
        Vertex{.position = {1.0F, -1.0F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .u = 1.0F, .v = 1.0F},
    };
}

std::vector<std::uint32_t> quad_indices() {
    // Two triangles covering the quad; cull_mode is NONE
    // (sdl3_mesh_pipeline.cpp), so winding order is not load-bearing here.
    return {0, 1, 2, 2, 1, 3};
}

// Mirrors the other GPU-dependent fixtures' own single-entry resource blob
// packer exactly (sdl3_frame_backend_test.cpp et al., itself mirroring
// tests/atlas-audio/decode_cache_test.cpp).
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

// The off-window render target's fixed size - small enough to keep interior
// sample pixels well away from bilinear-filtering edge artifacts at
// checker.tex's texture-quadrant boundaries (see sample_pixel() below), per
// the issue's own guidance.
constexpr Uint32 render_target_size = 8;
// A format this test controls explicitly end-to-end (render target texture
// creation, the mesh pipeline built against it, and this test's own
// pixel-byte-order assumption below) - deliberately not
// SDL_GetGPUSwapchainTextureFormat()'s own value (verified directly to be
// SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM under this sandbox's lavapipe/Vulkan
// combination): a pipeline built for one color target format must be used
// with a render target of that same format, and R8G8B8A8_UNORM's byte
// order (R, G, B, A - confirmed empirically via a standalone clear+download
// probe before writing this test) is simpler to reason about here than
// chasing whatever the swapchain happens to prefer on a given machine.
constexpr SDL_GPUTextureFormat render_target_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

// Reads one RGBA8 pixel out of a tightly-packed (no row padding - this
// test's own SDL_GPUTextureTransferInfo below sets pixels_per_row to
// render_target_size, matching decode_texture's own "no padding"
// convention) downloaded buffer.
Rgba sample_pixel(const std::vector<std::uint8_t>& pixels, Uint32 row, Uint32 col) {
    const std::size_t offset = (static_cast<std::size_t>(row) * render_target_size + col) * 4;
    return Rgba{
        .r = pixels.at(offset),
        .g = pixels.at(offset + 1),
        .b = pixels.at(offset + 2),
        .a = pixels.at(offset + 3),
    };
}

// Mirrors every other GPU-dependent fixture's own headless-CI pattern
// (sdl3_frame_backend_test.cpp's own doc comment has the full writeup of
// why "offscreen", not "dummy") - real SDL3/SDL_GPU window+device+mesh-
// pipeline construction in SetUp(), GTEST_SKIP() on any real failure
// (expected only on a machine with no Vulkan/Metal/D3D12 ICD at all).
class Sdl3PixelCorrectnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError();
        }

        window_ = SDL_CreateWindow("atlas-render-pixel-correctness-tests", 64, 64, SDL_WINDOW_HIDDEN);
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

        if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
            const std::string error = SDL_GetError();
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "SDL_ClaimWindowForGPUDevice failed: " << error;
        }

        try {
            pipeline_ = create_sdl3_mesh_pipeline(device_, render_target_format);
        } catch (const std::runtime_error& error) {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "create_sdl3_mesh_pipeline failed: " << error.what();
        }
    }

    void TearDown() override {
        destroy_sdl3_mesh_pipeline(device_, pipeline_);
        if (device_ != nullptr) {
            if (window_ != nullptr) {
                SDL_ReleaseWindowFromGPUDevice(device_, window_);
            }
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
    const Sdl3MeshPipeline& pipeline() { return pipeline_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    Sdl3MeshPipeline pipeline_;
};

TEST_F(Sdl3PixelCorrectnessTest,
       TexturedQuadDrawnThroughTheRealMeshPipelineProducesTheExactCheckerTexturePixelsPerQuadrant) {
    // Real resource resolution/decode/GPU-upload, through the same
    // MeshUploadCache/TextureUploadCache production code
    // Sdl3FrameBackend::submit() itself uses - not a reimplementation of
    // any part of that path.
    const ResourceId mesh_id = ResourceId::from_name("meshes/pixel-correctness/quad");
    const ResourceId material_id = ResourceId::from_name("textures/pixel-correctness/checker");
    const auto mesh_blob_path = write_temp_blob(
        "pixel_correctness_mesh.blob",
        pack_single_entry_blob(mesh_id, pack_decoded_mesh_bytes(quad_vertices(), quad_indices())));
    const auto texture_blob_path = write_temp_blob(
        "pixel_correctness_texture.blob", pack_single_entry_blob(material_id, read_fixture("checker.tex")));

    const resource::ResourceRegistry registry{{
        {"Mesh", mesh_blob_path},
        {"Texture", texture_blob_path},
    }};
    MeshUploadCache mesh_cache{registry, "Mesh", device()};
    TextureUploadCache texture_cache{registry, "Texture", device()};

    const MeshUploadResult& mesh = mesh_cache.get_or_upload(mesh_id);
    ASSERT_EQ(mesh.status, MeshUploadCacheStatus::Ok);
    ASSERT_NE(mesh.vertex_buffer, nullptr);
    ASSERT_NE(mesh.index_buffer, nullptr);
    ASSERT_EQ(mesh.index_count, 6U);

    const TextureUploadResult& texture = texture_cache.get_or_upload(material_id);
    ASSERT_EQ(texture.status, TextureUploadCacheStatus::Ok);
    ASSERT_NE(texture.texture, nullptr);
    ASSERT_EQ(texture.width, 2U);
    ASSERT_EQ(texture.height, 2U);

    // An explicit off-window render target - required under the
    // "offscreen" video driver (see this file's own top-of-file doc
    // comment): there is no real presentable swapchain surface to read
    // pixels back from.
    SDL_GPUTextureCreateInfo target_info{};
    target_info.type = SDL_GPU_TEXTURETYPE_2D;
    target_info.format = render_target_format;
    target_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    target_info.width = render_target_size;
    target_info.height = render_target_size;
    target_info.layer_count_or_depth = 1;
    target_info.num_levels = 1;
    target_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* target_texture = SDL_CreateGPUTexture(device(), &target_info);
    ASSERT_NE(target_texture, nullptr) << SDL_GetError();

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device());
    ASSERT_NE(command_buffer, nullptr) << SDL_GetError();

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = target_texture;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    // Clear to opaque black - not a color any sampled quadrant could
    // legitimately produce (see the four quadrant colors asserted below),
    // so any coverage gap in the quad would show up unambiguously rather
    // than accidentally matching a real texel.
    color_target.clear_color = SDL_FColor{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};

    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, nullptr);
    ASSERT_NE(render_pass, nullptr) << SDL_GetError();

    // The identity Transform: to_model_matrix() produces the identity
    // matrix, so mesh.vert.hlsl's `mul(Model, float4(Position, 1.0))`
    // leaves each vertex's authored clip-space Position untouched - exactly
    // the "no camera, whatever position you author is directly in clip
    // space" scope this issue documents.
    const std::array<float, 16> model_matrix = to_model_matrix(Transform{});

    draw_sdl3_mesh_pipeline(command_buffer,
                            render_pass,
                            pipeline(),
                            Sdl3MeshDrawInput{
                                .vertex_buffer = mesh.vertex_buffer,
                                .index_buffer = mesh.index_buffer,
                                .index_count = mesh.index_count,
                                .texture = texture.texture,
                            },
                            model_matrix);

    SDL_EndGPURenderPass(render_pass);

    constexpr Uint32 bytes_per_pixel = 4;
    constexpr Uint32 download_size = render_target_size * render_target_size * bytes_per_pixel;
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer_info.size = download_size;
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device(), &transfer_info);
    ASSERT_NE(transfer_buffer, nullptr) << SDL_GetError();

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    ASSERT_NE(copy_pass, nullptr) << SDL_GetError();

    SDL_GPUTextureRegion source_region{};
    source_region.texture = target_texture;
    source_region.w = render_target_size;
    source_region.h = render_target_size;
    source_region.d = 1;

    SDL_GPUTextureTransferInfo download_destination{};
    download_destination.transfer_buffer = transfer_buffer;
    download_destination.pixels_per_row = render_target_size;
    download_destination.rows_per_layer = render_target_size;

    SDL_DownloadFromGPUTexture(copy_pass, &source_region, &download_destination);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    ASSERT_NE(fence, nullptr) << SDL_GetError();
    SDL_WaitForGPUFences(device(), /*wait_all=*/true, &fence, 1);
    SDL_ReleaseGPUFence(device(), fence);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - the mapped memory is raw device bytes.
    const auto* mapped =
        static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device(), transfer_buffer, false));
    ASSERT_NE(mapped, nullptr) << SDL_GetError();
    const std::vector<std::uint8_t> pixels(mapped, mapped + download_size);
    SDL_UnmapGPUTransferBuffer(device(), transfer_buffer);
    SDL_ReleaseGPUTransferBuffer(device(), transfer_buffer);
    SDL_ReleaseGPUTexture(device(), target_texture);

    // Interior sample points, one per checker.tex quadrant, deliberately
    // chosen (columns/rows 1 and 6 of an 8-wide/tall target) so that
    // clamp-to-edge addressing collapses this texture's bilinear filter to
    // the exact texel color rather than a blend across the checker
    // boundary - worked out analytically before writing this assertion (a
    // linear-filtered sample at normalized coordinate u samples texel
    // space u*width - 0.5; for width 2 and u in {1,6}/8, that value falls
    // strictly inside [-1, 0) or [1, 2) respectively, where clamp-to-edge
    // makes both texels bilinear blends against reference the *same*
    // already-clamped index - never row/column 2..5, whose interpolated
    // texel-space coordinate straddles the real 0/1 texel boundary and
    // would genuinely blend two different colors).
    constexpr Uint32 near_edge = 1;
    constexpr Uint32 far_edge = 6;

    // Top-left quadrant: checker.tex's top-left texel - opaque red (per
    // texture_asset_test.cpp's own documented expectation).
    const Rgba top_left = sample_pixel(pixels, near_edge, near_edge);
    EXPECT_EQ(top_left.r, 255);
    EXPECT_EQ(top_left.g, 0);
    EXPECT_EQ(top_left.b, 0);
    EXPECT_EQ(top_left.a, 255);

    // Top-right quadrant: checker.tex's top-right texel - opaque green
    // (confirmed directly against the fixture's raw bytes; only the top-
    // left/bottom-right corners are asserted by texture_asset_test.cpp
    // itself, but all four are well-defined by decode_texture's own
    // row-major layout).
    const Rgba top_right = sample_pixel(pixels, near_edge, far_edge);
    EXPECT_EQ(top_right.r, 0);
    EXPECT_EQ(top_right.g, 255);
    EXPECT_EQ(top_right.b, 0);
    EXPECT_EQ(top_right.a, 255);

    // Bottom-left quadrant: checker.tex's bottom-left texel - opaque blue.
    const Rgba bottom_left = sample_pixel(pixels, far_edge, near_edge);
    EXPECT_EQ(bottom_left.r, 0);
    EXPECT_EQ(bottom_left.g, 0);
    EXPECT_EQ(bottom_left.b, 255);
    EXPECT_EQ(bottom_left.a, 255);

    // Bottom-right quadrant: checker.tex's bottom-right texel - translucent
    // yellow (per texture_asset_test.cpp's own documented expectation).
    // Written through unblended (this pipeline's color target blend state
    // is disabled - sdl3_mesh_pipeline.cpp), so the downloaded alpha is the
    // texture's own raw byte, not blended against the clear color.
    const Rgba bottom_right = sample_pixel(pixels, far_edge, far_edge);
    EXPECT_EQ(bottom_right.r, 255);
    EXPECT_EQ(bottom_right.g, 255);
    EXPECT_EQ(bottom_right.b, 0);
    EXPECT_EQ(bottom_right.a, 128);

    mesh_cache.release();
    texture_cache.release();
}

} // namespace
} // namespace atlas::render
