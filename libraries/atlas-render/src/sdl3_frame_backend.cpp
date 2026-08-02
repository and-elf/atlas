#include "atlas/render/sdl3_frame_backend.hpp"

#include <stdexcept>
#include <utility>

namespace atlas::render {

namespace {

// SDL_GPU picks the best available graphics backend per platform out of
// whichever shader formats it's told the application can supply
// (Vulkan/SPIR-V on Linux, Metal/MSL on macOS, D3D12/DXIL on Windows -
// exactly this project's three deployment targets, CLAUDE.md). This round
// never compiles or supplies an actual shader (no real geometry/shaders
// yet - #153/#154's job), so every format is offered; SDL_GPU only needs
// this list to pick a working backend, not to load a shader submitted with
// it.
constexpr SDL_GPUShaderFormat supported_shader_formats =
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

// The color this round's clear-and-present loop presents every frame -
// arbitrary, chosen only to be visibly non-black if ever actually displayed.
// Real content (#153/#154) replaces this outright; it is not a placeholder
// meant to survive.
constexpr SDL_FColor clear_color{.r = 0.10F, .g = 0.10F, .b = 0.12F, .a = 1.0F};

} // namespace

Sdl3FrameBackend::Sdl3FrameBackend(const std::string& window_title,
                                   int width,
                                   int height,
                                   SDL_WindowFlags extra_window_flags) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init(SDL_INIT_VIDEO) failed: ") + SDL_GetError());
    }

    window_ = SDL_CreateWindow(window_title.c_str(), width, height, extra_window_flags);
    if (window_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + error);
    }

    // debug_mode=false: validation-layer failures (e.g. no Vulkan validation
    // layers installed) are a distinct failure mode from "no working GPU
    // backend at all" and would otherwise muddy exactly the headless-CI
    // failure this constructor is meant to surface cleanly - see this
    // library's README for the documented headless-CI decision.
    device_ = SDL_CreateGPUDevice(supported_shader_formats, /*debug_mode=*/false, /*name=*/nullptr);
    if (device_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        throw std::runtime_error("SDL_CreateGPUDevice failed: " + error);
    }

    if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
        const std::string error = SDL_GetError();
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        throw std::runtime_error("SDL_ClaimWindowForGPUDevice failed: " + error);
    }

    // Issue #153: the one hardcoded triangle pipeline this round proves the
    // shader/pipeline/draw-call path with. Built once, here, rather than
    // lazily on first submit() - a failure here (bad shader compile, no
    // supported pipeline state) is exactly the same "this environment can't
    // actually do real GPU work" signal window/device construction already
    // reports, so it belongs in the same throwing constructor.
    try {
        triangle_pipeline_ =
            create_sdl3_triangle_pipeline(device_, SDL_GetGPUSwapchainTextureFormat(device_, window_));
    } catch (...) {
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        throw;
    }

    owns_sdl_ = true;
}

Sdl3FrameBackend::~Sdl3FrameBackend() {
    destroy();
}

Sdl3FrameBackend::Sdl3FrameBackend(Sdl3FrameBackend&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)),
      device_(std::exchange(other.device_, nullptr)),
      triangle_pipeline_(std::exchange(other.triangle_pipeline_, Sdl3TrianglePipeline{})),
      pending_(std::exchange(other.pending_, {})),
      last_completed_tick_(std::exchange(other.last_completed_tick_, std::nullopt)),
      owns_sdl_(std::exchange(other.owns_sdl_, false)) {}

Sdl3FrameBackend& Sdl3FrameBackend::operator=(Sdl3FrameBackend&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::exchange(other.window_, nullptr);
        device_ = std::exchange(other.device_, nullptr);
        triangle_pipeline_ = std::exchange(other.triangle_pipeline_, Sdl3TrianglePipeline{});
        pending_ = std::exchange(other.pending_, {});
        last_completed_tick_ = std::exchange(other.last_completed_tick_, std::nullopt);
        owns_sdl_ = std::exchange(other.owns_sdl_, false);
    }
    return *this;
}

void Sdl3FrameBackend::submit(const Frame& frame) {
    poll_completed_fences();

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    if (command_buffer == nullptr) {
        throw std::runtime_error(std::string("SDL_AcquireGPUCommandBuffer failed: ") + SDL_GetError());
    }

    Uint32 texture_width = 0;
    Uint32 texture_height = 0;
    SDL_GPUTexture* swapchain_texture = nullptr;
    // Wait-and-acquire (rather than the non-blocking acquire) matches
    // SDL_GPU's documented single-window frame-loop pattern. A false return
    // is a real error (SDL_GetError() explains it); a true return can still
    // legitimately hand back a null texture (e.g. the window has no
    // presentable swapchain image this frame, such as while minimized) -
    // not an error, just "skip the render pass, still submit."
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            command_buffer, window_, &swapchain_texture, &texture_width, &texture_height)) {
        throw std::runtime_error(std::string("SDL_WaitAndAcquireGPUSwapchainTexture failed: ") +
                                 SDL_GetError());
    }

    if (swapchain_texture != nullptr) {
        SDL_GPUColorTargetInfo color_target{};
        color_target.texture = swapchain_texture;
        color_target.clear_color = clear_color;
        color_target.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, nullptr);
        // Issue #153's one hardcoded draw call - proves the shader/pipeline/
        // draw-call path actually produces pixels, independent of
        // Frame::draw_commands (still ignored below - see this type's class
        // doc comment).
        draw_sdl3_triangle_pipeline(render_pass, triangle_pipeline_);
        SDL_EndGPURenderPass(render_pass);
    }

    // Frame::draw_commands is deliberately never consulted this round - see
    // this type's class doc comment and the library README's scoping note.

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (fence == nullptr) {
        throw std::runtime_error(std::string("SDL_SubmitGPUCommandBufferAndAcquireFence failed: ") +
                                 SDL_GetError());
    }

    pending_.push_back(PendingSubmission{.tick = frame.tick, .fence = fence});
}

std::optional<core::Time> Sdl3FrameBackend::last_completed_tick() {
    poll_completed_fences();
    return last_completed_tick_;
}

void Sdl3FrameBackend::poll_completed_fences() {
    auto it = pending_.begin();
    while (it != pending_.end()) {
        if (SDL_QueryGPUFence(device_, it->fence)) {
            if (!last_completed_tick_.has_value() || it->tick > *last_completed_tick_) {
                last_completed_tick_ = it->tick;
            }
            SDL_ReleaseGPUFence(device_, it->fence);
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}

void Sdl3FrameBackend::release_pending_fences_unconditionally() {
    for (const auto& submission : pending_) {
        SDL_ReleaseGPUFence(device_, submission.fence);
    }
    pending_.clear();
}

void Sdl3FrameBackend::destroy() noexcept {
    if (!owns_sdl_) {
        return;
    }

    if (device_ != nullptr) {
        // Releasing a device out from under GPU work that hasn't finished
        // yet is undefined per SDL_GPU's own contract, so every still-
        // pending fence is waited on (not merely dropped) before teardown.
        for (const auto& submission : pending_) {
            SDL_WaitForGPUFences(device_, /*wait_all=*/true, &submission.fence, 1);
        }
        release_pending_fences_unconditionally();

        // Only safe once every fence above has been waited on - the pipeline/
        // vertex buffer must outlive any GPU work that might still be
        // referencing them, and must be released before the device that
        // owns them is destroyed below.
        destroy_sdl3_triangle_pipeline(device_, triangle_pipeline_);

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

    SDL_Quit();
    owns_sdl_ = false;
}

} // namespace atlas::render
