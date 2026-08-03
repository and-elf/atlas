#pragma once

#include "atlas/render/transform.hpp"

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <span>

namespace atlas::render {

// GPU-driven distance culling (issue #156, the actual point of #117/#148's
// design): a compute pass decides, for every DrawCommand
// Sdl3FrameBackend::submit() has already resolved this frame, whether its
// Transform.position lies within a fixed maximum distance of a reference
// point, and writes a corresponding SDL_GPUIndexedIndirectDrawCommand into a
// caller-owned indirect-argument buffer - so the CPU never decides
// visibility itself, only iterates DrawCommands to bind each one's own
// (already-resolved) mesh/texture, exactly as it did before this issue.
//
// Distance culling, not frustum culling - locked in by the issue before
// implementation, not re-litigated here: no Camera/view-projection concept
// exists anywhere in Atlas yet (transform.hpp's to_model_matrix doc comment,
// this library's README "Open questions"), so true frustum culling has
// nothing to test against. Distance culling needs no camera at all and
// proves the real compute -> indirect-draw mechanism this issue exists for;
// true frustum/camera-aware culling remains a distinct, later follow-up.
//
// A basic aggregate (rule of zero): one raw SDL_GPU handle with no
// invariant beyond "released exactly once," matching Sdl3MeshPipeline's own
// precedent - deliberately not its own RAII class, for the same device-
// lifetime-ordering hazard Sdl3MeshPipeline's own doc comment documents:
// Sdl3FrameBackend already owns the SDL_GPUDevice this handle depends on and
// manages its own teardown order manually.
struct Sdl3DistanceCullPipeline {
    SDL_GPUComputePipeline* pipeline = nullptr;
};

// A reference point and maximum distance a DrawCommand's Transform.position
// is tested against - the entire "camera" this culling pass has, matching
// to_model_matrix's own "no camera exists yet, so nothing is composed
// against the model matrix" stance one level down: nothing here is derived
// from a view-projection either, just a fixed point and radius a caller
// chooses. Both fields are plain, caller-supplied configuration - never
// derived from wall-clock time or any other non-deterministic source (spec
// §4), though as presentation-only visibility culling (never simulation
// state) that constraint does not strictly apply here the way it does to
// Transform interpolation.
struct DistanceCullConfig {
    core::Vec3 reference_point{};
    // 1000.0F is a generously large default - large enough that every
    // existing DrawCommand in this library's own tests (all authored at or
    // near the origin) still survives unless a test opts into a tighter
    // value specifically to exercise culling.
    float max_distance = 1000.0F;
};

// One surviving-so-far DrawCommand's cull input: its Transform.position and
// its already-resolved mesh's index_count (carried through so the compute
// shader can populate SDL_GPUIndexedIndirectDrawCommand::num_indices without
// a second buffer or a second round-trip). Deliberately not DrawCommand or
// Transform themselves - this is the minimal, tightly-packed, GPU-upload-
// ready shape shaders/distance_cull.comp.hlsl's own StructuredBuffer<ObjectData>
// expects, matching decode_mesh's own "Vertex layout matches the consuming
// shader exactly" precedent (mesh_asset.hpp). A basic aggregate: 16 bytes,
// matching the HLSL ObjectData struct's own tight packing (float3 position +
// uint index_count) exactly - static_assert'd in the .cpp alongside the
// other GPU-visible layout types this library already asserts on.
struct DistanceCullObjectInput {
    float position_x = 0.0F;
    float position_y = 0.0F;
    float position_z = 0.0F;
    std::uint32_t index_count = 0;
};

// Every transient GPU resource one dispatch_sdl3_distance_cull() call
// creates - grouped so the caller (Sdl3FrameBackend::submit(), which owns
// the whole frame's single command-buffer submission) can release all three
// together, once, through release_sdl3_distance_cull_transients() below,
// after that same frame's SDL_SubmitGPUCommandBufferAndAcquireFence call has
// succeeded - never before (see dispatch_sdl3_distance_cull's own doc
// comment for why). A basic aggregate: three raw handles with no invariant
// of their own beyond "all released together, exactly once," the same
// rule-of-zero exception Sdl3MeshPipeline/Sdl3DistanceCullPipeline already
// take for GPU handles Sdl3FrameBackend manages the teardown order of
// manually.
struct Sdl3DistanceCullTransients {
    SDL_GPUBuffer* object_buffer = nullptr;
    SDL_GPUTransferBuffer* object_transfer_buffer = nullptr;
    SDL_GPUBuffer* indirect_buffer = nullptr;
};

// Compiles the checked-in compute HLSL (shaders/distance_cull.comp.hlsl,
// embedded as a constexpr std::string_view the same way
// sdl3_mesh_pipeline.cpp's graphics shaders are - see this library's
// CMakeLists.txt/cmake/EmbedShaderSource.cmake) via SDL_shadercross's
// compute-specific HLSL->SPIRV->SDL_GPUComputePipeline sequence
// (SDL_ShaderCross_CompileSPIRVFromHLSL with
// SDL_SHADERCROSS_SHADERSTAGE_COMPUTE -> SDL_ShaderCross_ReflectComputeSPIRV
// -> SDL_ShaderCross_CompileComputePipelineFromSPIRV) - verified against the
// real fetched SDL3_shadercross/SDL_shadercross.h to be a distinct, parallel
// three-call sequence to the graphics-shader one sdl3_mesh_pipeline.cpp
// already established (compute has its own reflection/compile entry points,
// not a mode of the graphics ones), not assumed. Throws std::runtime_error -
// matching every other SDL3-backend construction failure in this library -
// if shader compilation, reflection, or pipeline creation fails.
[[nodiscard]] Sdl3DistanceCullPipeline create_sdl3_distance_cull_pipeline(SDL_GPUDevice* device);

// Releases the compute pipeline and resets it to null - safe to call more
// than once, or on an already-null instance, mirroring
// destroy_sdl3_mesh_pipeline's own idempotency. Must be called before the
// owning SDL_GPUDevice is destroyed.
void destroy_sdl3_distance_cull_pipeline(SDL_GPUDevice* device, Sdl3DistanceCullPipeline& pipeline) noexcept;

// Runs one full distance-cull pass for this frame's `objects` (in the exact
// order the caller's own subsequent per-DrawCommand indirect draw calls must
// read them back in - objects[i] corresponds to entry i of the returned
// indirect buffer) within `command_buffer`: uploads `objects` to a freshly
// created SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ buffer (via a transfer
// buffer and a copy pass on the same command_buffer), then a compute pass
// that dispatches exactly enough workgroups to cover objects.size() (the
// shader itself bounds-checks past that via the input buffer's own real
// size - see the shader's own doc comment - not a separately-passed count
// that could fall out of sync), and returns a freshly created
// SDL_GPU_BUFFERUSAGE_INDIRECT | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
// buffer - deliberately never combined with SDL_GPU_BUFFERUSAGE_INDEX, see
// this library's README for the full SDL #14754 analysis this exact usage-
// flag combination is chosen to satisfy - containing objects.size()
// tightly-packed SDL_GPUIndexedIndirectDrawCommand-shaped entries. Entry i's
// num_instances is 1 when objects[i] survives the cull test (within
// config.max_distance of config.reference_point), 0 otherwise -
// SDL_DrawGPUIndexedPrimitivesIndirect against a 0-num_instances entry draws
// nothing and costs negligible GPU time, which is what lets this function's
// only caller (Sdl3FrameBackend::submit()) always issue one draw_count=1
// indirect draw per resolved DrawCommand with a statically-known draw_count,
// regardless of that DrawCommand's own cull outcome, rather than the CPU
// ever branching on visibility itself.
//
// objects must not be empty - the caller's own responsibility to check
// first (this function has no well-defined "cull nothing, dispatch zero
// workgroups" case to return).
//
// Every GPU resource this function creates is returned via
// Sdl3DistanceCullTransients and is only safe to release once
// command_buffer has actually been submitted (via
// SDL_SubmitGPUCommandBufferAndAcquireFence) - never before - the same
// "safe to release once the copy is submitted, SDL_GPU itself defers actual
// reclamation" contract mesh_upload_cache.cpp's own upload_gpu_buffer()
// documents, generalized here from "a transfer buffer specifically" to
// every SDL_GPU resource this function allocates.
//
// Throws std::runtime_error, matching every other SDL3-backend GPU failure
// in this library, if any GPU call this function makes fails - the caller
// is responsible for releasing whatever partial Sdl3DistanceCullTransients
// state existed before the throw (none - every partially-created handle is
// cleaned up internally before rethrowing, mirroring create_sdl3_mesh_pipeline's
// own all-or-nothing construction discipline).
[[nodiscard]] Sdl3DistanceCullTransients
dispatch_sdl3_distance_cull(SDL_GPUCommandBuffer* command_buffer,
                            SDL_GPUDevice* device,
                            const Sdl3DistanceCullPipeline& pipeline,
                            std::span<const DistanceCullObjectInput> objects,
                            const DistanceCullConfig& config);

// Releases every handle in transients and resets them all to null - safe to
// call more than once, or on a default-constructed/all-null instance. Must
// only be called once command_buffer's submission (see
// dispatch_sdl3_distance_cull's own doc comment above) has been accepted,
// i.e. after SDL_SubmitGPUCommandBufferAndAcquireFence returned successfully
// for the same command buffer this transients instance was produced from.
void release_sdl3_distance_cull_transients(SDL_GPUDevice* device,
                                           Sdl3DistanceCullTransients& transients) noexcept;

} // namespace atlas::render
