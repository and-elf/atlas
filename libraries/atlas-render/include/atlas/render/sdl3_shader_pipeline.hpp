#pragma once

#include <SDL3/SDL_gpu.h>

namespace atlas::render {

// A minimal SDL_GPU graphics pipeline plus the one hardcoded triangle's
// vertex buffer it draws (issue #153) - proving the shader/pipeline/
// draw-call path works end-to-end, independent of real Frame/DrawCommand/
// ResourceRegistry content (that remains issue #154's job, not this one's).
//
// A basic aggregate (rule of zero): both handles are raw SDL_GPU C-API
// handles with no invariant of their own beyond "both released exactly
// once," which destroy_sdl3_triangle_pipeline() below enforces explicitly.
// Deliberately NOT its own RAII class with a destructor: Sdl3FrameBackend
// already owns the SDL_GPUDevice these handles depend on and manages its
// own teardown manually (see its own class doc comment) - an automatically-
// invoked member destructor here could run after Sdl3FrameBackend::destroy()
// has already destroyed the device it needs, so this type's lifetime is
// managed the same explicit way window_/device_ already are.
struct Sdl3TrianglePipeline {
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    SDL_GPUBuffer* vertex_buffer = nullptr;
};

// Compiles the checked-in triangle vertex/fragment HLSL shaders
// (libraries/atlas-render/shaders/triangle.{vert,frag}.hlsl, embedded as
// constexpr std::string_view at build time - see this library's
// CMakeLists.txt and cmake/EmbedShaderSource.cmake) to SPIR-V via
// SDL_shadercross, builds a minimal SDL_GPUGraphicsPipeline targeting
// swapchain_format, and uploads one hardcoded triangle's vertex data
// (position + color, matching the vertex shader's TEXCOORD0/TEXCOORD1
// input layout - verified against real SDL_ShaderCross_ReflectGraphicsSPIRV
// output, see this library's README) to a new vertex-usage SDL_GPUBuffer.
//
// Compiled here, at construction time, rather than offline as a separate
// build step: this round has no offline shader-compiler tool of its own
// (that would be a new atlas- tool, arguably its own follow-up), and
// SDL_ShaderCross_CompileSPIRVFromHLSL() is fast enough (single-digit
// milliseconds for a shader this small) that doing it once per
// Sdl3FrameBackend construction is not a meaningful cost for this round's
// "prove the mechanism, not production-ready" bar - see this library's
// README, "Scoping decisions," for the full reasoning and what a real
// offline pipeline would need to change.
//
// Throws std::runtime_error - matching Sdl3FrameBackend's own convention
// (CLAUDE.md's documented std::expected incompatibility) - if shader
// compilation, reflection, shader/pipeline creation, or the vertex upload
// fails.
[[nodiscard]] Sdl3TrianglePipeline create_sdl3_triangle_pipeline(SDL_GPUDevice* device,
                                                                 SDL_GPUTextureFormat swapchain_format);

// Issues the one hardcoded draw call this pipeline exists for: binds the
// pipeline and its vertex buffer, then draws the (fixed, three-vertex)
// triangle. Must be called with an active SDL_GPURenderPass (see
// Sdl3FrameBackend::submit()).
void draw_sdl3_triangle_pipeline(SDL_GPURenderPass* render_pass, const Sdl3TrianglePipeline& pipeline);

// Releases both handles and resets pipeline to an all-null state - safe to
// call more than once, or with either/both handles already null, since
// Sdl3FrameBackend::destroy() itself may run more than once across
// move-assignment (see its own doc comment). Must be called before the
// owning SDL_GPUDevice is destroyed.
void destroy_sdl3_triangle_pipeline(SDL_GPUDevice* device, Sdl3TrianglePipeline& pipeline) noexcept;

} // namespace atlas::render
