#include "atlas/render/sdl3_mesh_pipeline.hpp"

#include "atlas/render/mesh_asset.hpp"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include "generated_shaders/mesh_frag_hlsl.hpp"
#include "generated_shaders/mesh_vert_hlsl.hpp"

namespace atlas::render {

namespace {

// Compiles one HLSL shader stage to a real SDL_GPUShader, via
// SDL_shadercross's documented HLSL->SPIRV->SDL_GPUShader sequence
// (SDL_ShaderCross_CompileSPIRVFromHLSL -> SDL_ShaderCross_ReflectGraphicsSPIRV
// -> SDL_ShaderCross_CompileGraphicsShaderFromSPIRV) - the same sequence
// issue #153's own compile_shader established, reproduced here rather than
// shared since that function lived in the now-deleted sdl3_shader_pipeline.cpp.
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

Sdl3MeshPipeline create_sdl3_mesh_pipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat swapchain_format) {
    if (!SDL_ShaderCross_Init()) {
        throw std::runtime_error(std::string("SDL_ShaderCross_Init failed: ") + SDL_GetError());
    }

    SDL_GPUShader* vertex_shader = nullptr;
    SDL_GPUShader* fragment_shader = nullptr;
    try {
        vertex_shader = compile_shader(device, shaders::mesh_vert_hlsl, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        fragment_shader =
            compile_shader(device, shaders::mesh_frag_hlsl, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
    } catch (...) {
        if (vertex_shader != nullptr) {
            SDL_ReleaseGPUShader(device, vertex_shader);
        }
        SDL_ShaderCross_Quit();
        throw;
    }

    // Matches decode_mesh's Vertex layout exactly (mesh_asset.hpp): Position
    // @0 (offset 0), Normal @1 (offset 12, unused by mesh.frag.hlsl - see
    // that shader's own doc comment for why the attribute is still declared
    // here), UV @2 (offset 24, a FLOAT2 covering both the u and v fields).
    const std::array<SDL_GPUVertexBufferDescription, 1> vertex_buffer_descriptions{{
        SDL_GPUVertexBufferDescription{
            .slot = 0,
            .pitch = sizeof(Vertex),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            .instance_step_rate = 0,
        },
    }};
    const std::array<SDL_GPUVertexAttribute, 3> vertex_attributes{{
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
            .offset = offsetof(Vertex, normal),
        },
        SDL_GPUVertexAttribute{
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(Vertex, u),
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

    // One shared sampler for every resolved texture this round - see
    // Sdl3MeshPipeline's own doc comment for why a single sampler suffices.
    // Linear filtering (smoother than nearest for a first real-content
    // round) and clamp-to-edge addressing (no wrapping assumption about
    // authored UVs) are the only two knobs this round has an opinion on;
    // mip biasing/anisotropy/comparison sampling are left at their
    // no-op defaults.
    const SDL_GPUSamplerCreateInfo sampler_info{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .mip_lod_bias = 0.0F,
        .max_anisotropy = 1.0F,
        .compare_op = SDL_GPU_COMPAREOP_NEVER,
        .min_lod = 0.0F,
        .max_lod = 0.0F,
        .enable_anisotropy = false,
        .enable_compare = false,
        .padding1 = 0,
        .padding2 = 0,
        .props = 0,
    };
    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (sampler == nullptr) {
        const std::string error = SDL_GetError();
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        throw std::runtime_error("SDL_CreateGPUSampler failed: " + error);
    }

    return Sdl3MeshPipeline{.pipeline = pipeline, .sampler = sampler};
}

namespace {

// The bind sequence draw_sdl3_mesh_pipeline and draw_sdl3_mesh_pipeline_indirect
// both need (pipeline, model-matrix uniform, vertex/index buffers, texture/
// sampler) - shared so the two draw call variants below differ only in
// their final SDL_DrawGPUIndexedPrimitives(Indirect) call.
void bind_sdl3_mesh_pipeline(SDL_GPUCommandBuffer* command_buffer,
                             SDL_GPURenderPass* render_pass,
                             const Sdl3MeshPipeline& pipeline,
                             const Sdl3MeshDrawInput& input,
                             const std::array<float, 16>& model_matrix) {
    SDL_BindGPUGraphicsPipeline(render_pass, pipeline.pipeline);

    SDL_PushGPUVertexUniformData(
        command_buffer, /*slot_index=*/0, model_matrix.data(), static_cast<Uint32>(sizeof(model_matrix)));

    const SDL_GPUBufferBinding vertex_buffer_binding{.buffer = input.vertex_buffer, .offset = 0};
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_buffer_binding, 1);

    const SDL_GPUBufferBinding index_buffer_binding{.buffer = input.index_buffer, .offset = 0};
    SDL_BindGPUIndexBuffer(render_pass, &index_buffer_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    const SDL_GPUTextureSamplerBinding texture_sampler_binding{.texture = input.texture,
                                                               .sampler = pipeline.sampler};
    SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_sampler_binding, 1);
}

} // namespace

void draw_sdl3_mesh_pipeline(SDL_GPUCommandBuffer* command_buffer,
                             SDL_GPURenderPass* render_pass,
                             const Sdl3MeshPipeline& pipeline,
                             const Sdl3MeshDrawInput& input,
                             const std::array<float, 16>& model_matrix) {
    bind_sdl3_mesh_pipeline(command_buffer, render_pass, pipeline, input, model_matrix);

    SDL_DrawGPUIndexedPrimitives(render_pass,
                                 input.index_count,
                                 /*num_instances=*/1,
                                 /*first_index=*/0,
                                 /*vertex_offset=*/0,
                                 /*first_instance=*/0);
}

void draw_sdl3_mesh_pipeline_indirect(SDL_GPUCommandBuffer* command_buffer,
                                      SDL_GPURenderPass* render_pass,
                                      const Sdl3MeshPipeline& pipeline,
                                      const Sdl3MeshDrawInput& input,
                                      const std::array<float, 16>& model_matrix,
                                      SDL_GPUBuffer* indirect_buffer,
                                      Uint32 indirect_offset) {
    bind_sdl3_mesh_pipeline(command_buffer, render_pass, pipeline, input, model_matrix);

    SDL_DrawGPUIndexedPrimitivesIndirect(render_pass, indirect_buffer, indirect_offset, /*draw_count=*/1);
}

void destroy_sdl3_mesh_pipeline(SDL_GPUDevice* device, Sdl3MeshPipeline& pipeline) noexcept {
    if (pipeline.sampler != nullptr) {
        SDL_ReleaseGPUSampler(device, pipeline.sampler);
        pipeline.sampler = nullptr;
    }
    if (pipeline.pipeline != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline.pipeline);
        pipeline.pipeline = nullptr;
    }
}

} // namespace atlas::render
