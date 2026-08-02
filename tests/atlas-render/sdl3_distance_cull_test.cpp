#include "atlas/render/sdl3_distance_cull_pipeline.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace atlas::render {
namespace {

// Issue #156: direct proof that dispatch_sdl3_distance_cull() writes the
// exact SDL_GPUIndexedIndirectDrawCommand contents this library's own
// integration (Sdl3FrameBackend::submit()) needs - a near object survives
// with num_instances=1 and its own mesh's num_indices, a far object is
// culled to num_instances=0 while still carrying its own num_indices
// through (never shrinking the output array itself - see this library's
// README, "Scoping decisions", for why the "how many survived" question
// this issue raises never needs its own separate mechanism under this
// design). Verifies the actual GPU-computed buffer bytes via the same
// transfer-buffer-download technique sdl3_pixel_correctness_test.cpp
// established for texture readback, applied here to a buffer instead
// (SDL_DownloadFromGPUBuffer rather than SDL_DownloadFromGPUTexture).
//
// Mirrors every other GPU-dependent fixture's own headless-CI pattern
// (sdl3_frame_backend_test.cpp's doc comment has the full "offscreen, not
// dummy" writeup) - real SDL3/SDL_GPU device construction in SetUp(),
// GTEST_SKIP() on any real failure.
class Sdl3DistanceCullTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError();
        }

        window_ = SDL_CreateWindow("atlas-render-distance-cull-tests", 64, 64, SDL_WINDOW_HIDDEN);
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
            pipeline_ = create_sdl3_distance_cull_pipeline(device_);
        } catch (const std::runtime_error& error) {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "create_sdl3_distance_cull_pipeline failed: " << error.what();
        }
    }

    void TearDown() override {
        destroy_sdl3_distance_cull_pipeline(device_, pipeline_);
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
    const Sdl3DistanceCullPipeline& pipeline() { return pipeline_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    Sdl3DistanceCullPipeline pipeline_;
};

// Matches SDL_GPUIndexedIndirectDrawCommand's real field layout exactly -
// this test's own readback struct, independent of the production one in
// sdl3_distance_cull_pipeline.cpp, matching this library's own established
// "each test defines its own small readback/packing helpers" precedent
// (sdl3_pixel_correctness_test.cpp's Rgba, sample_pixel()).
struct ReadBackIndirectCommand {
    std::uint32_t num_indices = 0;
    std::uint32_t num_instances = 0;
    std::uint32_t first_index = 0;
    std::int32_t vertex_offset = 0;
    std::uint32_t first_instance = 0;
};

std::vector<ReadBackIndirectCommand> run_cull_and_read_back(SDL_GPUDevice* device,
                                                            const Sdl3DistanceCullPipeline& pipeline,
                                                            std::span<const DistanceCullObjectInput> objects,
                                                            const DistanceCullConfig& config) {
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (command_buffer == nullptr) {
        ADD_FAILURE() << "SDL_AcquireGPUCommandBuffer failed: " << SDL_GetError();
        return {};
    }

    const Sdl3DistanceCullTransients transients =
        dispatch_sdl3_distance_cull(command_buffer, device, pipeline, objects, config);

    const auto download_size = static_cast<Uint32>(objects.size() * sizeof(ReadBackIndirectCommand));
    const SDL_GPUTransferBufferCreateInfo download_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = download_size, .props = 0};
    SDL_GPUTransferBuffer* download_buffer = SDL_CreateGPUTransferBuffer(device, &download_info);
    if (download_buffer == nullptr) {
        ADD_FAILURE() << "SDL_CreateGPUTransferBuffer failed: " << SDL_GetError();
        return {};
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (copy_pass == nullptr) {
        ADD_FAILURE() << "SDL_BeginGPUCopyPass failed: " << SDL_GetError();
        return {};
    }
    const SDL_GPUBufferRegion source{
        .buffer = transients.indirect_buffer, .offset = 0, .size = download_size};
    const SDL_GPUTransferBufferLocation destination{.transfer_buffer = download_buffer, .offset = 0};
    SDL_DownloadFromGPUBuffer(copy_pass, &source, &destination);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (fence == nullptr) {
        ADD_FAILURE() << "SDL_SubmitGPUCommandBufferAndAcquireFence failed: " << SDL_GetError();
        return {};
    }
    SDL_WaitForGPUFences(device, /*wait_all=*/true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);

    // Safe now - the command buffer that both wrote and read this data has
    // fully completed (see dispatch_sdl3_distance_cull's own doc comment).
    Sdl3DistanceCullTransients mutable_transients = transients;
    release_sdl3_distance_cull_transients(device, mutable_transients);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - the mapped memory is raw device bytes.
    const auto* mapped =
        static_cast<const ReadBackIndirectCommand*>(SDL_MapGPUTransferBuffer(device, download_buffer, false));
    if (mapped == nullptr) {
        ADD_FAILURE() << "SDL_MapGPUTransferBuffer failed: " << SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, download_buffer);
        return {};
    }
    std::vector<ReadBackIndirectCommand> result(mapped, mapped + objects.size());
    SDL_UnmapGPUTransferBuffer(device, download_buffer);
    SDL_ReleaseGPUTransferBuffer(device, download_buffer);
    return result;
}

TEST_F(Sdl3DistanceCullTest,
       ObjectWithinMaxDistanceSurvivesAndObjectBeyondMaxDistanceIsCulledToZeroInstances) {
    const std::array<DistanceCullObjectInput, 2> objects{{
        DistanceCullObjectInput{.position_x = 1.0F, .position_y = 0.0F, .position_z = 0.0F, .index_count = 6},
        DistanceCullObjectInput{
            .position_x = 500.0F, .position_y = 0.0F, .position_z = 0.0F, .index_count = 3},
    }};
    const DistanceCullConfig config{.reference_point = {0.0F, 0.0F, 0.0F}, .max_distance = 10.0F};

    const std::vector<ReadBackIndirectCommand> commands =
        run_cull_and_read_back(device(), pipeline(), objects, config);
    ASSERT_EQ(commands.size(), 2U);

    EXPECT_EQ(commands[0].num_instances, 1U);
    EXPECT_EQ(commands[0].num_indices, 6U);
    EXPECT_EQ(commands[0].first_index, 0U);
    EXPECT_EQ(commands[0].vertex_offset, 0);
    EXPECT_EQ(commands[0].first_instance, 0U);

    EXPECT_EQ(commands[1].num_instances, 0U);
    EXPECT_EQ(commands[1].num_indices, 3U);
}

TEST_F(Sdl3DistanceCullTest, ObjectExactlyAtTheReferencePointSurvivesAgainstAPositiveMaxDistance) {
    const std::array<DistanceCullObjectInput, 1> objects{{
        DistanceCullObjectInput{
            .position_x = 0.0F, .position_y = 0.0F, .position_z = 0.0F, .index_count = 36},
    }};
    const DistanceCullConfig config{.reference_point = {0.0F, 0.0F, 0.0F}, .max_distance = 1.0F};

    const std::vector<ReadBackIndirectCommand> commands =
        run_cull_and_read_back(device(), pipeline(), objects, config);
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(commands[0].num_instances, 1U);
    EXPECT_EQ(commands[0].num_indices, 36U);
}

TEST_F(Sdl3DistanceCullTest, MoreObjectsThanOneThreadWorkgroupAreAllCulledCorrectly) {
    // 96 objects (more than one 64-thread workgroup - shaders/distance_cull.comp.hlsl's
    // own [numthreads(64, 1, 1)]) exercises the multi-workgroup dispatch and
    // the shader's own Objects.GetDimensions() bounds check against a count
    // that is not an exact multiple of the workgroup size.
    constexpr std::size_t object_count = 96;
    std::vector<DistanceCullObjectInput> objects(object_count);
    for (std::size_t i = 0; i < object_count; ++i) {
        // Even indices near the reference point (survive), odd indices far
        // away (culled) - alternating so both outcomes are exercised across
        // both dispatched workgroups, not just the first or last one.
        const float x = (i % 2 == 0) ? 1.0F : 500.0F;
        objects[i] = DistanceCullObjectInput{.position_x = x,
                                             .position_y = 0.0F,
                                             .position_z = 0.0F,
                                             .index_count = static_cast<std::uint32_t>(i)};
    }
    const DistanceCullConfig config{.reference_point = {0.0F, 0.0F, 0.0F}, .max_distance = 10.0F};

    const std::vector<ReadBackIndirectCommand> commands =
        run_cull_and_read_back(device(), pipeline(), objects, config);
    ASSERT_EQ(commands.size(), object_count);
    for (std::size_t i = 0; i < object_count; ++i) {
        const std::uint32_t expected_instances = (i % 2 == 0) ? 1U : 0U;
        EXPECT_EQ(commands[i].num_instances, expected_instances) << "object " << i;
        EXPECT_EQ(commands[i].num_indices, static_cast<std::uint32_t>(i)) << "object " << i;
    }
}

} // namespace
} // namespace atlas::render
