// atlas-render (issue #154): real per-DrawCommand mesh vertex shader,
// superseding issue #153's hardcoded-triangle scaffolding (triangle.vert.hlsl,
// removed as part of this issue). Accepts Position/Normal/UV, matching
// decode_mesh's Vertex layout exactly (include/atlas/render/mesh_asset.hpp:
// { float3 position; float3 normal; float u, v; }, 32 bytes/vertex) - Normal
// is accepted but not read in this shader body (no lighting model exists yet,
// see this library's README), matching this project's own precedent of
// declaring a full input layout even when a particular shader variant does
// not consume every field.
//
// Compiled to SPIR-V at Sdl3FrameBackend construction time via
// SDL_shadercross's SDL_ShaderCross_CompileSPIRVFromHLSL() (see
// src/sdl3_mesh_pipeline.cpp) - plain HLSL, entry point "main", matching
// issue #153's established convention.
//
// TEXCOORD0/1/2 input semantics: SPIRV-Cross's reflection (via
// SDL_ShaderCross_ReflectGraphicsSPIRV) assigns locations by TEXCOORD index,
// not by post-dead-code-elimination declaration order - verified directly
// against real compiled+reflected output (Position/UV are both read by this
// shader and reflect at locations 0/2 respectively; Normal, unread, is
// elided from the reflected input list entirely, but the *pipeline's* vertex
// input state below still declares a location-1 attribute for it, which is
// harmless - SDL_GPU permits a bound vertex attribute a shader does not
// consume). This library's own vertex input state
// (src/sdl3_mesh_pipeline.cpp) matches: Position @0, Normal @1 (unused),
// UV @2.
//
// ModelUniform (b0, space1 - the SPIR-V resource-set convention
// SDL_CreateGPUShader's own doc comment documents for vertex-stage uniform
// buffers) carries this DrawCommand's model matrix, pushed once per draw via
// SDL_PushGPUVertexUniformData (verified against the real fetched SDL_gpu.h,
// not assumed - see this library's README).
//
// Issue #181: ViewProjectionUniform (b1, space1 - the next vertex-stage
// uniform-buffer slot after ModelUniform's b0, NOT space2, which this
// project's own real fetched SDL_gpu.h documents as the fragment stage's
// texture/sampler space, already occupied here by mesh.frag.hlsl's
// AlbedoTexture/AlbedoSampler - reusing it for a vertex-stage uniform buffer
// would collide with that convention, not merely with a specific already-
// used register) carries the active Camera's combined view-projection
// matrix (atlas::render::to_view_projection_matrix, camera.hpp), pushed once
// per frame via push_view_projection_uniform - SDL_GPU's own documented
// uniform-slot semantics ("data pushed to a slot... keeps its value
// throughout the command buffer until you push again", SDL_gpu.h) mean this
// does not need to be re-pushed alongside every per-DrawCommand model matrix
// the way ModelUniform is. output.Position composes both, ViewProjection
// applied last (outermost), against Model applied first (innermost) - a
// world-space point via Model, then transformed into clip space via
// ViewProjection.
//
// row_major: both matrices are uploaded from a plain, hand-rolled row-major
// std::array<float, 16> (atlas::render::to_model_matrix/
// to_view_projection_matrix, transform.hpp/camera.hpp) - this project takes
// no third-party math library dependency (matching its existing
// hand-rolled-format precedent, e.g. decode_mesh/decode_texture), so this
// annotation must match that layout exactly rather than relying on HLSL's
// own column-major default.
struct Input
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 UV       : TEXCOORD2;
};

struct Output
{
    float2 UV : TEXCOORD0;
    float4 Position : SV_Position;
};

cbuffer ModelUniform : register(b0, space1)
{
    row_major float4x4 Model;
};

cbuffer ViewProjectionUniform : register(b1, space1)
{
    row_major float4x4 ViewProjection;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(ViewProjection, mul(Model, float4(input.Position, 1.0)));
    output.UV = input.UV;
    return output;
}
