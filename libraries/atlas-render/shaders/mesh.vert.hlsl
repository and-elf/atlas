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
// not assumed - see this library's README). No camera/view-projection
// concept exists anywhere in Atlas yet (issue #154's own locked-in scope,
// deferred as a follow-up - see README "Open questions"): this shader applies
// the model matrix alone, with nothing composed against it - the simplest
// faithful expression of "no real camera this round," rather than
// multiplying by a separately-authored identity matrix that would only
// restate the same no-op.
//
// row_major: Model is uploaded from a plain, hand-rolled row-major
// std::array<float, 16> (atlas::render::to_model_matrix, transform.hpp) -
// this project takes no third-party math library dependency (matching its
// existing hand-rolled-format precedent, e.g. decode_mesh/decode_texture),
// so this annotation must match that layout exactly rather than relying on
// HLSL's own column-major default.
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

Output main(Input input)
{
    Output output;
    output.Position = mul(Model, float4(input.Position, 1.0));
    output.UV = input.UV;
    return output;
}
