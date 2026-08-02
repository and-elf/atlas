#include "atlas/render/sdl3_shader_pipeline.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::render {
namespace {

// Mirrors Sdl3FrameBackendTest's own headless-CI pattern (sdl3_frame_backend_test.cpp,
// issue #151's README decision): attempts *real* SDL3/SDL_GPU window+device
// creation in SetUp(), SDL_HINT_VIDEO_DRIVER forced to "dummy" so windowing
// itself always succeeds headlessly, and GTEST_SKIP()s with the real error
// the moment device creation fails (the expected outcome on this project's
// own sandboxed dev environment / most CI runners - no /dev/dri, no Vulkan
// ICD). Tested independently of Sdl3FrameBackend itself so a failure here
// isolates the shader/pipeline mechanism (issue #153) from window/device
// bring-up (issue #151).
class Sdl3ShaderPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError();
        }

        window_ = SDL_CreateWindow("atlas-render-shader-pipeline-tests", 64, 64, SDL_WINDOW_HIDDEN);
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
    }

    void TearDown() override {
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
    SDL_GPUTextureFormat swapchain_format() { return SDL_GetGPUSwapchainTextureFormat(device_, window_); }

private:
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
};

TEST_F(Sdl3ShaderPipelineTest, CreateProducesNonNullPipelineAndVertexBuffer) {
    Sdl3TrianglePipeline pipeline = create_sdl3_triangle_pipeline(device(), swapchain_format());

    EXPECT_NE(pipeline.pipeline, nullptr);
    EXPECT_NE(pipeline.vertex_buffer, nullptr);

    destroy_sdl3_triangle_pipeline(device(), pipeline);
}

TEST_F(Sdl3ShaderPipelineTest, DestroyResetsBothHandlesToNull) {
    Sdl3TrianglePipeline pipeline = create_sdl3_triangle_pipeline(device(), swapchain_format());

    destroy_sdl3_triangle_pipeline(device(), pipeline);

    EXPECT_EQ(pipeline.pipeline, nullptr);
    EXPECT_EQ(pipeline.vertex_buffer, nullptr);
}

TEST_F(Sdl3ShaderPipelineTest, DestroyIsIdempotentAndSafeToCallTwice) {
    Sdl3TrianglePipeline pipeline = create_sdl3_triangle_pipeline(device(), swapchain_format());

    destroy_sdl3_triangle_pipeline(device(), pipeline);
    // Reaching this line without a double-free crash (ASan/UBSan enabled in
    // the debug preset) is the assertion - the second call must be a no-op
    // against the already-nulled handles, not a double release.
    EXPECT_NO_THROW(destroy_sdl3_triangle_pipeline(device(), pipeline));
}

TEST_F(Sdl3ShaderPipelineTest, DrawInsideARealRenderPassDoesNotThrowOrCrash) {
    Sdl3TrianglePipeline pipeline = create_sdl3_triangle_pipeline(device(), swapchain_format());

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device());
    ASSERT_NE(command_buffer, nullptr) << SDL_GetError();

    SDL_GPUColorTargetInfo color_target{};
    // A plain, otherwise-unused offscreen texture as the render target - this
    // test proves the draw call itself (issue #153's whole point: the
    // pipeline/draw-call path actually executes without error), not that a
    // specific window's swapchain receives it; Sdl3FrameBackendTest already
    // covers the full swapchain-acquire/present loop end-to-end.
    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = swapchain_format();
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = 64;
    texture_info.height = 64;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* target_texture = SDL_CreateGPUTexture(device(), &texture_info);
    ASSERT_NE(target_texture, nullptr) << SDL_GetError();

    color_target.texture = target_texture;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = SDL_FColor{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};

    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, nullptr);
    ASSERT_NE(render_pass, nullptr) << SDL_GetError();

    EXPECT_NO_THROW(draw_sdl3_triangle_pipeline(render_pass, pipeline));

    SDL_EndGPURenderPass(render_pass);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    ASSERT_NE(fence, nullptr) << SDL_GetError();
    SDL_WaitForGPUFences(device(), /*wait_all=*/true, &fence, 1);
    SDL_ReleaseGPUFence(device(), fence);

    SDL_ReleaseGPUTexture(device(), target_texture);
    destroy_sdl3_triangle_pipeline(device(), pipeline);
}

} // namespace
} // namespace atlas::render
