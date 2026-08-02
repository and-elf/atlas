#include "atlas/render/sdl3_distance_cull_pipeline.hpp"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

#include "generated_shaders/distance_cull_comp_hlsl.hpp"

namespace atlas::render {

namespace {

// Matches SDL_GPUIndexedIndirectDrawCommand's real field layout exactly
// (SDL_gpu.h, verified against the real fetched header) - the buffer
// dispatch_sdl3_distance_cull() returns must consist of tightly-packed
// entries of exactly this shape, per SDL_DrawGPUIndexedPrimitivesIndirect's
// own documented contract.
struct IndirectDrawCommand {
    std::uint32_t num_indices = 0;
    std::uint32_t num_instances = 0;
    std::uint32_t first_index = 0;
    std::int32_t vertex_offset = 0;
    std::uint32_t first_instance = 0;
};
static_assert(sizeof(IndirectDrawCommand) == 20,
              "IndirectDrawCommand must match SDL_GPUIndexedIndirectDrawCommand's tightly-packed layout");
static_assert(sizeof(DistanceCullObjectInput) == 16,
              "DistanceCullObjectInput must match distance_cull.comp.hlsl's ObjectData layout exactly");

// The cbuffer CullParams : register(b0, space2) layout in
// shaders/distance_cull.comp.hlsl: float3 ReferencePoint followed by float
// MaxDistance packs into a single 16-byte HLSL constant register (a scalar
// immediately after a float3 shares that register's fourth component,
// HLSL's own default cbuffer packing rule - the same "vec3/vec4 fields must
// be 16-byte aligned" std140-shaped requirement SDL_PushGPUComputeUniformData's
// own doc comment states), matching this plain 16-byte struct exactly.
struct CullParamsUniform {
    float reference_x = 0.0F;
    float reference_y = 0.0F;
    float reference_z = 0.0F;
    float max_distance = 0.0F;
};
static_assert(sizeof(CullParamsUniform) == 16);

// 64 threads per workgroup - must match shaders/distance_cull.comp.hlsl's
// own [numthreads(64, 1, 1)] declaration exactly (SDL_GPUComputePipelineCreateInfo's
// threadcount_x/y/z "should match the value in the shader", per the real
// fetched SDL_gpu.h - reflection fills these in from the compiled shader
// itself, see create_sdl3_distance_cull_pipeline below, so this constant is
// only needed here, for computing how many workgroups a given object count
// needs).
constexpr Uint32 threads_per_workgroup = 64;

// Uploads `data` (size_bytes) into a fresh GPU buffer of `usage`, via a
// fresh transfer buffer and a copy pass on `command_buffer` - deliberately
// NOT its own command-buffer-acquire-and-submit round trip, unlike
// mesh_upload_cache.cpp's own upload_gpu_buffer(): this function is always
// called from inside dispatch_sdl3_distance_cull(), which shares its
// caller's (Sdl3FrameBackend::submit()'s) single per-frame command buffer
// so the copy pass's buffer write and the following compute pass's read
// stay properly ordered/barriered within one submission, rather than
// racing two independently-submitted command buffers. Returns both the
// buffer and the transfer buffer it was uploaded through (the transfer
// buffer must stay alive until this same command buffer is submitted - see
// this file's own release_sdl3_distance_cull_transients doc comment).
// Throws std::runtime_error on any failure, cleaning up whatever partial
// state it created first.
struct UploadedBuffer {
    SDL_GPUBuffer* buffer = nullptr;
    SDL_GPUTransferBuffer* transfer_buffer = nullptr;
};

UploadedBuffer upload_gpu_buffer_on_shared_command_buffer(SDL_GPUDevice* device,
                                                          SDL_GPUCommandBuffer* command_buffer,
                                                          SDL_GPUBufferUsageFlags usage,
                                                          const void* data,
                                                          Uint32 size_bytes) {
    const SDL_GPUBufferCreateInfo buffer_info{.usage = usage, .size = size_bytes, .props = 0};
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (buffer == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateGPUBuffer failed: ") + SDL_GetError());
    }

    const SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size_bytes, .props = 0};
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer_buffer == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUBuffer(device, buffer);
        throw std::runtime_error("SDL_CreateGPUTransferBuffer failed: " + error);
    }

    void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, /*cycle=*/false);
    if (mapped == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        throw std::runtime_error("SDL_MapGPUTransferBuffer failed: " + error);
    }
    std::memcpy(mapped, data, size_bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (copy_pass == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        throw std::runtime_error("SDL_BeginGPUCopyPass failed: " + error);
    }

    const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
    const SDL_GPUBufferRegion destination{.buffer = buffer, .offset = 0, .size = size_bytes};
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, /*cycle=*/false);
    SDL_EndGPUCopyPass(copy_pass);

    return UploadedBuffer{.buffer = buffer, .transfer_buffer = transfer_buffer};
}

} // namespace

Sdl3DistanceCullPipeline create_sdl3_distance_cull_pipeline(SDL_GPUDevice* device) {
    if (!SDL_ShaderCross_Init()) {
        throw std::runtime_error(std::string("SDL_ShaderCross_Init failed: ") + SDL_GetError());
    }

    SDL_ShaderCross_HLSL_Info hlsl_info{};
    // shaders::distance_cull_comp_hlsl is a NUL-terminated embedded raw
    // string literal (EmbedShaderSource.cmake), same as sdl3_mesh_pipeline.cpp's
    // own two shaders - .data() is safe to treat as a C string.
    hlsl_info.source = shaders::distance_cull_comp_hlsl.data();
    hlsl_info.entrypoint = "main";
    hlsl_info.include_dir = nullptr;
    hlsl_info.defines = nullptr;
    hlsl_info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;
    hlsl_info.props = 0;

    size_t spirv_size = 0;
    void* spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl_info, &spirv_size);
    if (spirv == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ShaderCross_Quit();
        throw std::runtime_error("SDL_ShaderCross_CompileSPIRVFromHLSL failed: " + error);
    }

    SDL_ShaderCross_ComputePipelineMetadata* metadata =
        SDL_ShaderCross_ReflectComputeSPIRV(static_cast<const Uint8*>(spirv), spirv_size, 0);
    if (metadata == nullptr) {
        const std::string error = SDL_GetError();
        SDL_free(spirv);
        SDL_ShaderCross_Quit();
        throw std::runtime_error("SDL_ShaderCross_ReflectComputeSPIRV failed: " + error);
    }

    SDL_ShaderCross_SPIRV_Info spirv_info{};
    spirv_info.bytecode = static_cast<const Uint8*>(spirv);
    spirv_info.bytecode_size = spirv_size;
    spirv_info.entrypoint = "main";
    spirv_info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;
    spirv_info.props = 0;

    SDL_GPUComputePipeline* pipeline =
        SDL_ShaderCross_CompileComputePipelineFromSPIRV(device, &spirv_info, metadata, 0);

    const std::string compile_error = pipeline == nullptr ? SDL_GetError() : std::string{};

    SDL_free(metadata);
    SDL_free(spirv);
    SDL_ShaderCross_Quit();

    if (pipeline == nullptr) {
        throw std::runtime_error("SDL_ShaderCross_CompileComputePipelineFromSPIRV failed: " + compile_error);
    }

    return Sdl3DistanceCullPipeline{.pipeline = pipeline};
}

void destroy_sdl3_distance_cull_pipeline(SDL_GPUDevice* device, Sdl3DistanceCullPipeline& pipeline) noexcept {
    if (pipeline.pipeline != nullptr) {
        SDL_ReleaseGPUComputePipeline(device, pipeline.pipeline);
        pipeline.pipeline = nullptr;
    }
}

Sdl3DistanceCullTransients dispatch_sdl3_distance_cull(SDL_GPUCommandBuffer* command_buffer,
                                                       SDL_GPUDevice* device,
                                                       const Sdl3DistanceCullPipeline& pipeline,
                                                       std::span<const DistanceCullObjectInput> objects,
                                                       const DistanceCullConfig& config) {
    const auto object_count = static_cast<Uint32>(objects.size());
    const auto object_bytes = static_cast<Uint32>(objects.size_bytes());

    const UploadedBuffer object_upload = upload_gpu_buffer_on_shared_command_buffer(
        device, command_buffer, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, objects.data(), object_bytes);

    const auto indirect_bytes = static_cast<Uint32>(objects.size() * sizeof(IndirectDrawCommand));
    // Never combined with SDL_GPU_BUFFERUSAGE_INDEX - see this library's
    // README for the full SDL #14754 analysis this exact flag combination
    // is chosen to satisfy (VULKAN_INTERNAL_DefaultBufferUsageMode checks
    // INDEX before INDIRECT; a buffer with only these two flags set never
    // reaches that ambiguous case at all).
    const SDL_GPUBufferCreateInfo indirect_buffer_info{.usage = SDL_GPU_BUFFERUSAGE_INDIRECT |
                                                                SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
                                                       .size = indirect_bytes,
                                                       .props = 0};
    SDL_GPUBuffer* indirect_buffer = SDL_CreateGPUBuffer(device, &indirect_buffer_info);
    if (indirect_buffer == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, object_upload.transfer_buffer);
        SDL_ReleaseGPUBuffer(device, object_upload.buffer);
        throw std::runtime_error("SDL_CreateGPUBuffer (indirect buffer) failed: " + error);
    }

    const SDL_GPUStorageBufferReadWriteBinding indirect_binding{
        .buffer = indirect_buffer, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0};
    SDL_GPUComputePass* compute_pass =
        SDL_BeginGPUComputePass(command_buffer, nullptr, 0, &indirect_binding, 1);
    if (compute_pass == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUBuffer(device, indirect_buffer);
        SDL_ReleaseGPUTransferBuffer(device, object_upload.transfer_buffer);
        SDL_ReleaseGPUBuffer(device, object_upload.buffer);
        throw std::runtime_error("SDL_BeginGPUComputePass failed: " + error);
    }

    SDL_BindGPUComputePipeline(compute_pass, pipeline.pipeline);
    const std::array<SDL_GPUBuffer*, 1> readonly_storage_buffers{object_upload.buffer};
    SDL_BindGPUComputeStorageBuffers(compute_pass, 0, readonly_storage_buffers.data(), 1);

    const CullParamsUniform uniform{
        .reference_x = config.reference_point.x,
        .reference_y = config.reference_point.y,
        .reference_z = config.reference_point.z,
        .max_distance = config.max_distance,
    };
    SDL_PushGPUComputeUniformData(
        command_buffer, /*slot_index=*/0, &uniform, static_cast<Uint32>(sizeof(uniform)));

    const Uint32 groupcount_x = (object_count + threads_per_workgroup - 1) / threads_per_workgroup;
    SDL_DispatchGPUCompute(compute_pass, groupcount_x, 1, 1);

    SDL_EndGPUComputePass(compute_pass);

    return Sdl3DistanceCullTransients{
        .object_buffer = object_upload.buffer,
        .object_transfer_buffer = object_upload.transfer_buffer,
        .indirect_buffer = indirect_buffer,
    };
}

void release_sdl3_distance_cull_transients(SDL_GPUDevice* device,
                                           Sdl3DistanceCullTransients& transients) noexcept {
    if (transients.object_transfer_buffer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, transients.object_transfer_buffer);
        transients.object_transfer_buffer = nullptr;
    }
    if (transients.object_buffer != nullptr) {
        SDL_ReleaseGPUBuffer(device, transients.object_buffer);
        transients.object_buffer = nullptr;
    }
    if (transients.indirect_buffer != nullptr) {
        SDL_ReleaseGPUBuffer(device, transients.indirect_buffer);
        transients.indirect_buffer = nullptr;
    }
}

} // namespace atlas::render
