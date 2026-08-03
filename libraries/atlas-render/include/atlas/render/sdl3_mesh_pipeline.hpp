#pragma once

#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstdint>

namespace atlas::render {

// A real SDL_GPU graphics pipeline for drawing real per-DrawCommand mesh
// content (issue #154) - superseding issue #153's Sdl3TrianglePipeline
// (removed as part of this issue), which proved the shader/pipeline/
// draw-call mechanism with one hardcoded triangle and nothing else.
//
// A basic aggregate (rule of zero): both handles are raw SDL_GPU C-API
// handles with no invariant of their own beyond "both released exactly
// once," which destroy_sdl3_mesh_pipeline() below enforces explicitly.
// Deliberately NOT its own RAII class with a destructor - the same
// device-lifetime-ordering hazard Sdl3TrianglePipeline's own doc comment
// documented applies here unchanged: Sdl3FrameBackend already owns the
// SDL_GPUDevice these handles depend on and manages its own teardown
// manually, so an automatically-invoked member destructor here could run
// after that device has already been destroyed.
struct Sdl3MeshPipeline {
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    // One shared sampler for every draw this round - sampling *state*
    // (filtering/addressing), unlike a texture's own pixel content, has no
    // per-resource variation yet (no per-material sampler configuration
    // exists - see this library's README, "Open questions"), so a single
    // sampler serves every resolved texture.
    SDL_GPUSampler* sampler = nullptr;
};

// The GPU-side inputs one draw_sdl3_mesh_pipeline() call needs, resolved
// ahead of time by the caller (Sdl3FrameBackend::submit()) via
// MeshUploadCache/TextureUploadCache - kept free of either cache type's own
// headers so this module stays a pure "given these handles, draw" boundary
// (CLAUDE.md, Architecture Principles: "clear boundaries between layers").
struct Sdl3MeshDrawInput {
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer = nullptr;
    std::uint32_t index_count = 0;
    SDL_GPUTexture* texture = nullptr;
};

// Compiles the checked-in mesh vertex/fragment HLSL shaders
// (libraries/atlas-render/shaders/mesh.{vert,frag}.hlsl, embedded as
// constexpr std::string_view at build time - see this library's
// CMakeLists.txt and cmake/EmbedShaderSource.cmake) to SPIR-V via
// SDL_shadercross, the same HLSL->SPIRV->SDL_GPUShader sequence issue #153
// established, builds a graphics pipeline whose vertex input state matches
// decode_mesh's Vertex layout exactly (mesh_asset.hpp: Position @0,
// Normal @1, UV @2 - see mesh.vert.hlsl's own doc comment for why Normal's
// attribute is declared here despite being unread by the shader), and
// creates one shared linear-filtering, clamp-to-edge SDL_GPUSampler.
//
// Throws std::runtime_error - matching Sdl3FrameBackend's own convention
// (CLAUDE.md's documented std::expected incompatibility) - if shader
// compilation, reflection, shader/pipeline, or sampler creation fails.
[[nodiscard]] Sdl3MeshPipeline create_sdl3_mesh_pipeline(SDL_GPUDevice* device,
                                                         SDL_GPUTextureFormat swapchain_format);

// Issues one real, textured, transformed draw call: binds the pipeline,
// pushes model_matrix as this draw's vertex uniform (ModelUniform,
// mesh.vert.hlsl), binds input's vertex/index buffers and its texture
// (paired with this pipeline's own shared sampler), and issues one indexed
// draw over input.index_count indices - decode_mesh produces indices, and
// this function always uses them, never SDL_DrawGPUPrimitives' unindexed
// form. Must be called with an active SDL_GPURenderPass, and command_buffer
// must be the same command buffer that render_pass was begun on (SDL_GPU's
// own per-draw uniform push targets the command buffer, not the render
// pass - see SDL_PushGPUVertexUniformData's own doc comment in the real
// fetched SDL_gpu.h).
void draw_sdl3_mesh_pipeline(SDL_GPUCommandBuffer* command_buffer,
                             SDL_GPURenderPass* render_pass,
                             const Sdl3MeshPipeline& pipeline,
                             const Sdl3MeshDrawInput& input,
                             const std::array<float, 16>& model_matrix);

// Issue #156: the same bind sequence as draw_sdl3_mesh_pipeline (pipeline,
// model-matrix uniform, vertex/index buffers, texture/sampler), but issues
// SDL_DrawGPUIndexedPrimitivesIndirect against indirect_buffer at
// indirect_offset (bytes) instead of SDL_DrawGPUIndexedPrimitives - reading
// exactly one SDL_GPUIndexedIndirectDrawCommand-shaped entry (draw_count=1),
// written ahead of time by a distance-cull compute pass
// (sdl3_distance_cull_pipeline.hpp) into indirect_buffer, which decides that
// entry's own num_instances (0 or 1). Always issued regardless of whether
// this particular DrawCommand actually survived culling - the CPU never
// branches on visibility itself; a 0-num_instances entry draws nothing and
// costs negligible GPU time (see this library's README, "Scoping
// decisions"). Must be called with an active SDL_GPURenderPass, the same
// contract draw_sdl3_mesh_pipeline documents, and indirect_buffer must have
// been created with SDL_GPU_BUFFERUSAGE_INDIRECT (SDL_DrawGPUIndexedPrimitivesIndirect's
// own documented requirement).
void draw_sdl3_mesh_pipeline_indirect(SDL_GPUCommandBuffer* command_buffer,
                                      SDL_GPURenderPass* render_pass,
                                      const Sdl3MeshPipeline& pipeline,
                                      const Sdl3MeshDrawInput& input,
                                      const std::array<float, 16>& model_matrix,
                                      SDL_GPUBuffer* indirect_buffer,
                                      Uint32 indirect_offset);

// Issue #181: pushes the active Camera's combined view-projection matrix
// (atlas::render::to_view_projection_matrix, camera.hpp) as
// ViewProjectionUniform (mesh.vert.hlsl, b1/space1 - the vertex-stage
// uniform-buffer slot immediately after ModelUniform's own b0/space1, slot
// index 1). Unlike model_matrix (pushed once per draw by
// draw_sdl3_mesh_pipeline(_indirect) below, since it differs per
// DrawCommand), the view-projection matrix is the same for every draw in a
// frame - SDL_GPU's own documented uniform-slot semantics ("Uniform data
// pushed to a slot... keeps its value throughout the command buffer until
// you call the relevant Push function on that slot again", the real fetched
// SDL_gpu.h) mean this only needs to be called once per command buffer,
// before the first draw call that reads it, rather than once per draw the
// way ModelUniform is. Must be called on the same command_buffer every
// subsequent draw_sdl3_mesh_pipeline(_indirect) call within this frame
// targets, the same per-command-buffer contract those two functions'
// model_matrix push already documents.
void push_view_projection_uniform(SDL_GPUCommandBuffer* command_buffer,
                                  const std::array<float, 16>& view_projection_matrix);

// Releases both handles and resets pipeline to an all-null state - safe to
// call more than once, or with either/both handles already null, mirroring
// destroy_sdl3_triangle_pipeline()'s own idempotency (Sdl3FrameBackend's own
// destroy() may run more than once across move-assignment). Must be called
// before the owning SDL_GPUDevice is destroyed.
void destroy_sdl3_mesh_pipeline(SDL_GPUDevice* device, Sdl3MeshPipeline& pipeline) noexcept;

} // namespace atlas::render
