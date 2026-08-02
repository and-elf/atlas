// atlas-render (issue #153): minimal hardcoded-triangle vertex shader, proving
// the shader/pipeline/draw-call path end-to-end - not real Frame/DrawCommand
// content (that's issue #154).
//
// Compiled to SPIR-V at Sdl3FrameBackend construction time via
// SDL_shadercross's SDL_ShaderCross_CompileSPIRVFromHLSL() (see
// src/sdl3_shader_pipeline.cpp) - plain HLSL, entry point "main", no
// [shader("vertex")] attribute needed, matching SDL_shadercross's own
// test/shaders/simple.vert.hlsl convention (verified against the real file
// at this project's pinned SDL_shadercross commit, not assumed).
//
// TEXCOORD0/TEXCOORD1 input semantics: SPIRV-Cross's reflection (via
// SDL_ShaderCross_ReflectGraphicsSPIRV) assigns these vertex input locations
// 0 and 1 respectively, in declaration order - verified directly against the
// real compiled+reflected output, not assumed from HLSL convention alone.
// This library's own Vertex layout (sdl3_shader_pipeline.cpp) matches: a
// packed { float3 position; float3 color; } matching TEXCOORD0/TEXCOORD1
// in that order.
struct Input
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
};

struct Output
{
    float3 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.Position = float4(input.Position, 1.0);
    output.Color = input.Color;
    return output;
}
