// atlas-render (issue #153): minimal hardcoded-triangle fragment shader -
// passes the vertex-interpolated color straight through to SV_Target0. See
// triangle.vert.hlsl for the input convention this must match (TEXCOORD0 ==
// the vertex shader's Color output, both verified against real compiled +
// reflected SPIR-V, not assumed).
struct Input
{
    float3 Color : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    return float4(input.Color, 1.0);
}
