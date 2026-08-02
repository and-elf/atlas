// atlas-render (issue #156): GPU-driven distance-culling compute shader.
// For every object in Objects (one entry per surviving-so-far DrawCommand -
// see include/atlas/render/sdl3_distance_cull_pipeline.hpp's own doc
// comment), tests whether its Position lies within MaxDistance of
// ReferencePoint and writes a corresponding SDL_GPUIndexedIndirectDrawCommand-
// shaped entry into Commands: NumInstances is 1 when the object survives, 0
// otherwise - never skipping or shrinking the output array itself, so the
// CPU-issued indirect draw call always has a statically-known draw_count
// (see this library's README, "Scoping decisions", for why this round
// issues one draw_count=1 indirect draw per resolved DrawCommand rather
// than one draw_count=N call spanning every DrawCommand in the Frame: each
// mesh has its own separate vertex/index buffer (MeshUploadCache, issue
// #154), and a single indirect multi-draw call can only span entries that
// share one bound vertex/index buffer - batching heterogeneous meshes into
// one shared buffer is a distinct, unrequested mesh-batching redesign this
// issue does not attempt).
//
// Distance culling, not frustum culling - a real, standalone GPU culling
// technique needing no camera/view-projection concept at all (none exists
// anywhere in Atlas yet - transform.hpp's to_model_matrix doc comment).
// True frustum culling is a distinct, later follow-up once a real Camera
// type lands (this library's README, "Open questions").
//
// Resource bindings follow SDL_CreateGPUComputePipeline's own documented
// SPIR-V resource-set convention (SDL_gpu.h): set 0 (readonly storage
// buffers), set 1 (read-write storage buffers), set 2 (uniform buffers) -
// register(space) maps directly onto the SPIR-V descriptor set number for
// this project's own SDL_shadercross invocation (no -fvk-*-shift arguments
// passed - the same finding this library's README documents for
// mesh.vert.hlsl/mesh.frag.hlsl's own register/space choices).
struct ObjectData
{
    float3 Position;
    uint IndexCount;
};

// Matches SDL_GPUIndexedIndirectDrawCommand's real field layout exactly
// (SDL_gpu.h: num_indices, num_instances, first_index, vertex_offset,
// first_instance - verified against the real fetched header, not assumed).
struct IndirectDrawCommand
{
    uint NumIndices;
    uint NumInstances;
    uint FirstIndex;
    int VertexOffset;
    uint FirstInstance;
};

StructuredBuffer<ObjectData> Objects : register(t0, space0);
RWStructuredBuffer<IndirectDrawCommand> Commands : register(u0, space1);

cbuffer CullParams : register(b0, space2)
{
    float3 ReferencePoint;
    float MaxDistance;
};

// 64 threads per workgroup - a conventional compute-shader group size with
// no particular tuning behind it yet (this round proves the mechanism, not
// a tuned production dispatch - matching this library's own "prove the
// mechanism, not production-ready" bar elsewhere, e.g. HLSL compiled at
// runtime rather than offline).
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Objects.GetDimensions, not a separately-passed object count: the
    // input buffer this round always holds exactly as many entries as this
    // frame's resolved DrawCommand count (sdl3_distance_cull_pipeline.cpp
    // allocates it freshly, sized exactly, every frame - see that file's
    // own doc comment), so its own real size is already the one true bound
    // to guard the dispatch's own thread-count rounding-up against, with
    // nothing else needing to stay in sync with it.
    uint object_count = 0;
    uint object_stride = 0;
    Objects.GetDimensions(object_count, object_stride);

    uint index = id.x;
    if (index >= object_count)
    {
        return;
    }

    float3 position = Objects[index].Position;
    float3 delta = position - ReferencePoint;
    float distance_squared = dot(delta, delta);
    bool visible = distance_squared <= (MaxDistance * MaxDistance);

    IndirectDrawCommand command;
    command.NumIndices = Objects[index].IndexCount;
    command.NumInstances = visible ? 1u : 0u;
    command.FirstIndex = 0;
    command.VertexOffset = 0;
    command.FirstInstance = 0;

    Commands[index] = command;
}
