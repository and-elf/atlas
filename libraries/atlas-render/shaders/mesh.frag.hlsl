// atlas-render (issue #154): real per-DrawCommand mesh fragment shader,
// superseding issue #153's hardcoded-triangle scaffolding (triangle.frag.hlsl,
// removed as part of this issue). Samples a bound texture via UV - no
// lighting model, no material parameters beyond the one bound albedo
// texture (see this library's README, "Open questions").
//
// AlbedoTexture/AlbedoSampler (t0/s0, space2 - the SPIR-V resource-set
// convention SDL_CreateGPUShader's own doc comment documents for
// fragment-stage sampled textures) are bound once per draw
// (SDL_BindGPUFragmentSamplers, src/sdl3_mesh_pipeline.cpp) to the
// TextureUploadCache's resolved SDL_GPUTexture plus one shared sampler this
// library owns (Sdl3MeshPipeline::sampler - texture sampling *state*, unlike
// the texture's own pixel content, has no per-resource variation this round,
// so one shared sampler for every draw is sufficient).
struct Input
{
    float2 UV : TEXCOORD0;
};

Texture2D<float4> AlbedoTexture : register(t0, space2);
SamplerState AlbedoSampler : register(s0, space2);

float4 main(Input input) : SV_Target0
{
    return AlbedoTexture.Sample(AlbedoSampler, input.UV);
}
