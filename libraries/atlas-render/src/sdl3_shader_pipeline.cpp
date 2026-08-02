#include "atlas/render/sdl3_shader_pipeline.hpp"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include "generated_shaders/triangle_frag_hlsl.hpp"
#include "generated_shaders/triangle_vert_hlsl.hpp"

namespace atlas::render {

namespace {

// Position + color, matching triangle.vert.hlsl's TEXCOORD0/TEXCOORD1 input
// layout exactly (location 0 == Position, location 1 == Color) - verified
// against real SDL_ShaderCross_ReflectGraphicsSPIRV output, not assumed; see
// this library's README, "Scoping decisions."
struct Vertex {
    std::array<float, 3> position;
    std::array<float, 3> color;
};

// One hardcoded triangle (issue #153's whole point - not real geometry, see
// this library's README) in NDC clip space, one vertex per primary color so
// a real draw is visually unambiguous if ever actually displayed.
constexpr std::array<Vertex, 3> triangle_vertices{{
    Vertex{.position = {0.0F, 0.5F, 0.0F}, .color = {1.0F, 0.0F, 0.0F}},
    Vertex{.position = {0.5F, -0.5F, 0.0F}, .color = {0.0F, 1.0F, 0.0F}},
    Vertex{.position = {-0.5F, -0.5F, 0.0F}, .color = {0.0F, 0.0F, 1.0F}},
}};

// Compiles one HLSL shader stage to a real SDL_GPUShader, via
// SDL_shadercross's documented HLSL->SPIRV->SDL_GPUShader sequence
// (SDL_ShaderCross_CompileSPIRVFromHLSL -> SDL_ShaderCross_ReflectGraphicsSPIRV
// -> SDL_ShaderCross_CompileGraphicsShaderFromSPIRV) - verified against the
// real fetched header (include/SDL3_shadercross/SDL_shadercross.h at this
// project's pinned commit), not paraphrased from memory.
SDL_GPUShader*
compile_shader(SDL_GPUDevice* device, std::string_view hlsl_source, SDL_ShaderCross_ShaderStage stage) {
    SDL_ShaderCross_HLSL_Info hlsl_info{};
    // hlsl_source is always one of the two embedded, NUL-terminated raw
    // string literals below (EmbedShaderSource.cmake) - .data() is safe to
    // treat as a C string.
    hlsl_info.source = hlsl_source.data();
    hlsl_info.entrypoint = "main";
    hlsl_info.include_dir = nullptr;
    hlsl_info.defines = nullptr;
    hlsl_info.shader_stage = stage;
    hlsl_info.props = 0;

    size_t spirv_size = 0;
    void* spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl_info, &spirv_size);
    if (spirv == nullptr) {
        throw std::runtime_error(std::string("SDL_ShaderCross_CompileSPIRVFromHLSL failed: ") +
                                 SDL_GetError());
    }

    SDL_ShaderCross_GraphicsShaderMetadata* metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(spirv), spirv_size, 0);
    if (metadata == nullptr) {
        const std::string error = SDL_GetError();
        SDL_free(spirv);
        throw std::runtime_error("SDL_ShaderCross_ReflectGraphicsSPIRV failed: " + error);
    }

    SDL_ShaderCross_SPIRV_Info spirv_info{};
    spirv_info.bytecode = static_cast<const Uint8*>(spirv);
    spirv_info.bytecode_size = spirv_size;
    spirv_info.entrypoint = "main";
    spirv_info.shader_stage = stage;
    spirv_info.props = 0;

    SDL_GPUShader* shader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &spirv_info, &metadata->resource_info, 0);

    const std::string compile_error = shader == nullptr ? SDL_GetError() : std::string{};

    SDL_free(metadata);
    SDL_free(spirv);

    if (shader == nullptr) {
        throw std::runtime_error("SDL_ShaderCross_CompileGraphicsShaderFromSPIRV failed: " + compile_error);
    }

    return shader;
}

} // namespace

Sdl3TrianglePipeline create_sdl3_triangle_pipeline(SDL_GPUDevice* device,
                                                   SDL_GPUTextureFormat swapchain_format) {
    if (!SDL_ShaderCross_Init()) {
        throw std::runtime_error(std::string("SDL_ShaderCross_Init failed: ") + SDL_GetError());
    }

    SDL_GPUShader* vertex_shader = nullptr;
    SDL_GPUShader* fragment_shader = nullptr;
    try {
        vertex_shader =
            compile_shader(device, shaders::triangle_vert_hlsl, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        fragment_shader =
            compile_shader(device, shaders::triangle_frag_hlsl, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
    } catch (...) {
        if (vertex_shader != nullptr) {
            SDL_ReleaseGPUShader(device, vertex_shader);
        }
        SDL_ShaderCross_Quit();
        throw;
    }

    const std::array<SDL_GPUVertexBufferDescription, 1> vertex_buffer_descriptions{{
        SDL_GPUVertexBufferDescription{
            .slot = 0,
            .pitch = sizeof(Vertex),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            .instance_step_rate = 0,
        },
    }};
    const std::array<SDL_GPUVertexAttribute, 2> vertex_attributes{{
        SDL_GPUVertexAttribute{
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(Vertex, position),
        },
        SDL_GPUVertexAttribute{
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(Vertex, color),
        },
    }};
    const std::array<SDL_GPUColorTargetDescription, 1> color_target_descriptions{{
        SDL_GPUColorTargetDescription{.format = swapchain_format, .blend_state = {}},
    }};

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions.data();
    pipeline_info.vertex_input_state.num_vertex_buffers =
        static_cast<Uint32>(vertex_buffer_descriptions.size());
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes.data();
    pipeline_info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(vertex_attributes.size());
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline_info.target_info.color_target_descriptions = color_target_descriptions.data();
    pipeline_info.target_info.num_color_targets = static_cast<Uint32>(color_target_descriptions.size());

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);

    // Shaders are only needed at pipeline-creation time (SDL_GPU's own
    // documented convention) - released immediately after regardless of
    // success.
    SDL_ReleaseGPUShader(device, vertex_shader);
    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ShaderCross_Quit();

    if (pipeline == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateGPUGraphicsPipeline failed: ") + SDL_GetError());
    }

    constexpr auto vertex_buffer_size = static_cast<Uint32>(sizeof(Vertex) * triangle_vertices.size());

    const SDL_GPUBufferCreateInfo buffer_info{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = vertex_buffer_size,
        .props = 0,
    };
    SDL_GPUBuffer* vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (vertex_buffer == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_CreateGPUBuffer failed: " + error);
    }

    const SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = vertex_buffer_size,
        .props = 0,
    };
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer_buffer == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_CreateGPUTransferBuffer failed: " + error);
    }

    void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, /*cycle=*/false);
    if (mapped == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_MapGPUTransferBuffer failed: " + error);
    }
    std::memcpy(mapped, triangle_vertices.data(), vertex_buffer_size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCommandBuffer* upload_command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (upload_command_buffer == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_AcquireGPUCommandBuffer (vertex upload) failed: " + error);
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);
    if (copy_pass == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_BeginGPUCopyPass failed: " + error);
    }

    const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
    const SDL_GPUBufferRegion destination{.buffer = vertex_buffer, .offset = 0, .size = vertex_buffer_size};
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, /*cycle=*/false);
    SDL_EndGPUCopyPass(copy_pass);

    // Submitted immediately (rather than folding into the caller's own
    // per-frame submit()) so this one-time upload's lifetime is entirely
    // self-contained here - Sdl3FrameBackend::submit() never needs to know
    // the vertex buffer was ever unpopulated.
    if (!SDL_SubmitGPUCommandBuffer(upload_command_buffer)) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_SubmitGPUCommandBuffer (vertex upload) failed: " + error);
    }

    // Safe to release once the copy is submitted - SDL_GPU itself defers
    // actual reclamation until the submitted work is safe to reclaim.
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);

    return Sdl3TrianglePipeline{.pipeline = pipeline, .vertex_buffer = vertex_buffer};
}

void draw_sdl3_triangle_pipeline(SDL_GPURenderPass* render_pass, const Sdl3TrianglePipeline& pipeline) {
    SDL_BindGPUGraphicsPipeline(render_pass, pipeline.pipeline);

    const SDL_GPUBufferBinding vertex_buffer_binding{.buffer = pipeline.vertex_buffer, .offset = 0};
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_buffer_binding, 1);

    SDL_DrawGPUPrimitives(render_pass,
                          static_cast<Uint32>(triangle_vertices.size()),
                          /*num_instances=*/1,
                          /*first_vertex=*/0,
                          /*first_instance=*/0);
}

void destroy_sdl3_triangle_pipeline(SDL_GPUDevice* device, Sdl3TrianglePipeline& pipeline) noexcept {
    if (pipeline.vertex_buffer != nullptr) {
        SDL_ReleaseGPUBuffer(device, pipeline.vertex_buffer);
        pipeline.vertex_buffer = nullptr;
    }
    if (pipeline.pipeline != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline.pipeline);
        pipeline.pipeline = nullptr;
    }
}

} // namespace atlas::render
