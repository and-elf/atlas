#include "atlas/render/sdl3_frame_backend.hpp"

#include "atlas/render/transform.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace atlas::render {

namespace {

// SDL_GPU picks the best available graphics backend per platform out of
// whichever shader formats it's told the application can supply
// (Vulkan/SPIR-V on Linux, Metal/MSL on macOS, D3D12/DXIL on Windows -
// exactly this project's three deployment targets, CLAUDE.md).
constexpr SDL_GPUShaderFormat supported_shader_formats =
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

// The color this round's clear-and-present loop presents every frame -
// arbitrary, chosen only to be visibly non-black if ever actually
// displayed, before any real DrawCommand content is drawn over it.
constexpr SDL_FColor clear_color{.r = 0.10F, .g = 0.10F, .b = 0.12F, .a = 1.0F};

// The two ResourceRegistry asset-type names this backend's caches use - the
// established convention, confirmed in
// tests/atlas-resource/resource_registry_test.cpp.
constexpr std::string_view mesh_type_name = "Mesh";
constexpr std::string_view texture_type_name = "Texture";

// One DrawCommand whose mesh/material both resolved this frame (issue
// #154) - gathered up front (submit(), below) before the distance-cull
// compute pass runs, so cull_inputs (submit()'s own local) stays index-
// aligned with this list: entry i's cull outcome (written by the compute
// pass into the shared indirect buffer) belongs to resolved_draws[i]'s own
// subsequent indirect draw call.
struct ResolvedDraw {
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer = nullptr;
    SDL_GPUTexture* texture = nullptr;
    std::array<float, 16> model_matrix{};
};

} // namespace

// Claims window for a real SDL_GPUDevice and builds this backend's
// pipelines/caches against it - shared between both constructors below.
// Never touches window's own lifetime (creation/destruction) or SDL's video
// subsystem init: on failure it unwinds only what it itself allocated
// (device claim, device, pipelines) and rethrows, leaving window exactly as
// it found it - each constructor layers its own window/SDL_Init ownership
// handling (or lack thereof) around this call.
void Sdl3FrameBackend::create_device_and_pipelines(SDL_Window* window,
                                                   const resource::ResourceRegistry& registry) {
    // debug_mode=false: validation-layer failures (e.g. no Vulkan validation
    // layers installed) are a distinct failure mode from "no working GPU
    // backend at all" and would otherwise muddy exactly the headless-CI
    // failure this constructor is meant to surface cleanly - see this
    // library's README for the documented headless-CI decision.
    device_ = SDL_CreateGPUDevice(supported_shader_formats, /*debug_mode=*/false, /*name=*/nullptr);
    if (device_ == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError());
    }

    if (!SDL_ClaimWindowForGPUDevice(device_, window)) {
        const std::string error = SDL_GetError();
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        throw std::runtime_error("SDL_ClaimWindowForGPUDevice failed: " + error);
    }

    // Issue #154: the real mesh pipeline this round's real Frame/DrawCommand
    // content is drawn with. Built once, here, rather than lazily on first
    // submit() - a failure here (bad shader compile, no supported pipeline
    // state) is exactly the same "this environment can't actually do real
    // GPU work" signal window/device construction already reports, so it
    // belongs in the same throwing constructor.
    try {
        mesh_pipeline_ =
            create_sdl3_mesh_pipeline(device_, SDL_GetGPUSwapchainTextureFormat(device_, window));
    } catch (...) {
        SDL_ReleaseWindowFromGPUDevice(device_, window);
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        throw;
    }

    // Issue #156: the real GPU-driven distance-cull compute pipeline every
    // submit() now runs. Built once, here, alongside mesh_pipeline_ above,
    // for the same reason - a shader/pipeline build failure is the same
    // "this environment can't actually do real GPU work" signal as every
    // other construction failure in this constructor.
    try {
        distance_cull_pipeline_ = create_sdl3_distance_cull_pipeline(device_);
    } catch (...) {
        destroy_sdl3_mesh_pipeline(device_, mesh_pipeline_);
        SDL_ReleaseWindowFromGPUDevice(device_, window);
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        throw;
    }

    mesh_cache_.emplace(registry, mesh_type_name, device_);
    texture_cache_.emplace(registry, texture_type_name, device_);
}

Sdl3FrameBackend::Sdl3FrameBackend(const resource::ResourceRegistry& registry,
                                   const std::string& window_title,
                                   int width,
                                   int height,
                                   SDL_WindowFlags extra_window_flags,
                                   DistanceCullConfig distance_cull,
                                   Camera camera)
    : distance_cull_config_(distance_cull), camera_(camera) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init(SDL_INIT_VIDEO) failed: ") + SDL_GetError());
    }

    window_ = SDL_CreateWindow(window_title.c_str(), width, height, extra_window_flags);
    if (window_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + error);
    }
    owns_window_ = true;

    try {
        create_device_and_pipelines(window_, registry);
    } catch (...) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        throw;
    }

    owns_sdl_ = true;
}

// owns_window_ is left at its default-member-initializer value (false) -
// this constructor never owns window_.
Sdl3FrameBackend::Sdl3FrameBackend(const resource::ResourceRegistry& registry,
                                   windowing::Sdl3SharedWindow& shared_window,
                                   DistanceCullConfig distance_cull,
                                   Camera camera)
    : window_(shared_window.handle()), distance_cull_config_(distance_cull), camera_(camera) {
    // No try/catch needed here: create_device_and_pipelines already unwinds
    // everything it itself allocated on failure and rethrows, and this
    // constructor owns nothing else (window/SDL_Init both remain
    // shared_window's responsibility either way) for it to additionally
    // clean up.
    create_device_and_pipelines(window_, registry);

    // Must stay a body assignment - it reflects whether
    // create_device_and_pipelines above actually succeeded, so it cannot run
    // before that fallible call the way a member initializer would.
    // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
    owns_sdl_ = true;
}

Sdl3FrameBackend::~Sdl3FrameBackend() {
    destroy();
}

Sdl3FrameBackend::Sdl3FrameBackend(Sdl3FrameBackend&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)),
      device_(std::exchange(other.device_, nullptr)),
      mesh_pipeline_(std::exchange(other.mesh_pipeline_, Sdl3MeshPipeline{})),
      distance_cull_pipeline_(std::exchange(other.distance_cull_pipeline_, Sdl3DistanceCullPipeline{})),
      distance_cull_config_(std::exchange(other.distance_cull_config_, DistanceCullConfig{})),
      camera_(std::exchange(other.camera_, Camera{})),
      mesh_cache_(std::exchange(other.mesh_cache_, std::nullopt)),
      texture_cache_(std::exchange(other.texture_cache_, std::nullopt)),
      pending_(std::exchange(other.pending_, {})),
      last_completed_tick_(std::exchange(other.last_completed_tick_, std::nullopt)),
      owns_sdl_(std::exchange(other.owns_sdl_, false)),
      owns_window_(std::exchange(other.owns_window_, false)) {}

Sdl3FrameBackend& Sdl3FrameBackend::operator=(Sdl3FrameBackend&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::exchange(other.window_, nullptr);
        device_ = std::exchange(other.device_, nullptr);
        mesh_pipeline_ = std::exchange(other.mesh_pipeline_, Sdl3MeshPipeline{});
        distance_cull_pipeline_ = std::exchange(other.distance_cull_pipeline_, Sdl3DistanceCullPipeline{});
        distance_cull_config_ = std::exchange(other.distance_cull_config_, DistanceCullConfig{});
        camera_ = std::exchange(other.camera_, Camera{});
        mesh_cache_ = std::exchange(other.mesh_cache_, std::nullopt);
        texture_cache_ = std::exchange(other.texture_cache_, std::nullopt);
        pending_ = std::exchange(other.pending_, {});
        last_completed_tick_ = std::exchange(other.last_completed_tick_, std::nullopt);
        owns_sdl_ = std::exchange(other.owns_sdl_, false);
        owns_window_ = std::exchange(other.owns_window_, false);
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

    Sdl3DistanceCullTransients cull_transients;
    if (swapchain_texture != nullptr) {
        // Issue #181: the active camera's combined view-projection matrix is
        // the same for every draw this frame, so it is pushed once per
        // command buffer here - never re-pushed per DrawCommand the way
        // each draw's own model matrix is (see push_view_projection_uniform's
        // own doc comment for why SDL_GPU's uniform-slot semantics make that
        // safe).
        push_view_projection_uniform(command_buffer, to_view_projection_matrix(camera_));

        // Issue #154/#156: resolve every DrawCommand's mesh/material first,
        // in Frame::draw_commands's own order - a DrawCommand whose mesh or
        // texture failed to resolve/decode/upload is skipped outright, never
        // substituted or coerced, matching build_frame's own documented skip
        // convention (frame_builder.hpp) and this project's established
        // "skip, never substitute" stance throughout. resolved_draws and
        // cull_inputs stay index-aligned: cull_inputs[i]'s computed
        // visibility (below) corresponds to resolved_draws[i]'s own
        // indirect draw call.
        std::vector<ResolvedDraw> resolved_draws;
        std::vector<DistanceCullObjectInput> cull_inputs;
        resolved_draws.reserve(frame.draw_commands.size());
        cull_inputs.reserve(frame.draw_commands.size());

        for (const DrawCommand& draw_command : frame.draw_commands) {
            const MeshUploadResult& mesh = mesh_cache_->get_or_upload(draw_command.mesh);
            if (mesh.status != MeshUploadCacheStatus::Ok || mesh.vertex_buffer == nullptr ||
                mesh.index_buffer == nullptr) {
                continue;
            }

            const TextureUploadResult& texture = texture_cache_->get_or_upload(draw_command.material);
            if (texture.status != TextureUploadCacheStatus::Ok || texture.texture == nullptr) {
                continue;
            }

            resolved_draws.push_back(ResolvedDraw{
                .vertex_buffer = mesh.vertex_buffer,
                .index_buffer = mesh.index_buffer,
                .texture = texture.texture,
                .model_matrix = to_model_matrix(draw_command.transform),
            });
            cull_inputs.push_back(DistanceCullObjectInput{
                .position_x = draw_command.transform.position.x,
                .position_y = draw_command.transform.position.y,
                .position_z = draw_command.transform.position.z,
                .index_count = mesh.index_count,
            });
        }

        // Issue #156: a real compute pass decides visibility for every
        // resolved DrawCommand at once, on the GPU - the CPU below never
        // branches on distance itself, only iterates resolved_draws to bind
        // each one's own (already-resolved) mesh/texture, exactly as it did
        // before this issue.
        if (!resolved_draws.empty()) {
            cull_transients = dispatch_sdl3_distance_cull(
                command_buffer, device_, distance_cull_pipeline_, cull_inputs, distance_cull_config_);
        }

        SDL_GPUColorTargetInfo color_target{};
        color_target.texture = swapchain_texture;
        color_target.clear_color = clear_color;
        color_target.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, nullptr);

        // One draw_count=1 indirect draw per resolved DrawCommand, always
        // issued regardless of that entry's own cull outcome - the compute
        // pass above already decided (and wrote) its num_instances (0 or 1)
        // into cull_transients.indirect_buffer; a 0-num_instances entry
        // draws nothing and costs negligible GPU time (see this library's
        // README, "Scoping decisions", for why this round issues one
        // draw_count=1 call per DrawCommand rather than one draw_count=N
        // call spanning every DrawCommand in the Frame).
        for (std::size_t i = 0; i < resolved_draws.size(); ++i) {
            const ResolvedDraw& draw = resolved_draws[i];
            const auto indirect_offset = static_cast<Uint32>(i * sizeof(SDL_GPUIndexedIndirectDrawCommand));

            draw_sdl3_mesh_pipeline_indirect(command_buffer,
                                             render_pass,
                                             mesh_pipeline_,
                                             Sdl3MeshDrawInput{
                                                 .vertex_buffer = draw.vertex_buffer,
                                                 .index_buffer = draw.index_buffer,
                                                 .index_count = 0, // unused - the indirect entry supplies it.
                                                 .texture = draw.texture,
                                             },
                                             draw.model_matrix,
                                             cull_transients.indirect_buffer,
                                             indirect_offset);
        }

        SDL_EndGPURenderPass(render_pass);
    }

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (fence == nullptr) {
        throw std::runtime_error(std::string("SDL_SubmitGPUCommandBufferAndAcquireFence failed: ") +
                                 SDL_GetError());
    }

    // Safe now - command_buffer (which both wrote and read every transient
    // cull resource) has been submitted (dispatch_sdl3_distance_cull's own
    // doc comment). A no-op on a default-constructed (all-null) instance,
    // i.e. whenever this frame had nothing to resolve/cull.
    release_sdl3_distance_cull_transients(device_, cull_transients);

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

        // Only safe once every fence above has been waited on - every GPU
        // buffer/texture/sampler/pipeline handle below must outlive any GPU
        // work that might still be referencing it, and must be released
        // before the device that owns it is destroyed. Caches first (they
        // hold buffers/textures the pipeline's draw calls referenced), then
        // the pipeline itself - mirrors destroy_sdl3_triangle_pipeline's own
        // explicit-teardown-before-device-destruction discipline (see
        // MeshUploadCache/TextureUploadCache's own doc comments for why
        // release() exists instead of relying on their destructors).
        if (texture_cache_.has_value()) {
            texture_cache_->release();
        }
        if (mesh_cache_.has_value()) {
            mesh_cache_->release();
        }
        destroy_sdl3_mesh_pipeline(device_, mesh_pipeline_);
        destroy_sdl3_distance_cull_pipeline(device_, distance_cull_pipeline_);

        if (window_ != nullptr) {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
        }
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
    }

    if (owns_window_) {
        // Self-contained construction (this instance created window_ and
        // called SDL_Init(SDL_INIT_VIDEO) itself) - it alone is responsible
        // for tearing both back down.
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    }
    // Shared-window construction (issue #174): window_ is borrowed from a
    // windowing::Sdl3SharedWindow this instance does not own - its
    // destruction/SDL_Quit() remain entirely that owner's responsibility.

    window_ = nullptr;
    owns_sdl_ = false;
    owns_window_ = false;
}

} // namespace atlas::render
