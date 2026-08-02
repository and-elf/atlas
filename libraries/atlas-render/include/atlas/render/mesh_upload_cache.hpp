#pragma once

#include "atlas/render/mesh_asset.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atlas::render {

// Distinguishes every way resolving+decoding+uploading a mesh can fail,
// mirroring atlas::audio::DecodeCacheStatus's own split (issue #166, this
// type's direct template) plus one addition this cache needs that audio's
// CPU-only cache does not: UploadFailed, for a real SDL_GPU buffer-creation/
// upload failure (distinct from decode_mesh returning std::nullopt, which is
// a CPU-side format problem, not a GPU one).
enum class MeshUploadCacheStatus : std::uint8_t {
    Ok,
    Unresolved,       // ResourceRegistry: type never registered, id absent, or a null id
    ResolutionFailed, // ResourceRegistry: the type was registered but its blob failed to load
    DecodeFailed,     // decode_mesh(): malformed or truncated mesh bytes
    UploadFailed,     // a real SDL_GPU buffer create/map/upload call failed
};

// vertex_buffer/index_buffer/index_count are only meaningful when
// status == Ok, matching atlas::audio::DecodeCacheResult's own "payload
// meaningful only on success" convention. A mesh that decodes to zero
// vertices/indices (decode_mesh's own well-formed "empty mesh" case) is
// still Ok, with both buffer pointers left null and index_count == 0 -
// Sdl3FrameBackend::submit() skips issuing a draw call for it, the same
// "skip, never substitute" handling as an outright failure, just for a
// different underlying reason.
struct MeshUploadResult {
    MeshUploadCacheStatus status = MeshUploadCacheStatus::Unresolved;
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer = nullptr;
    std::uint32_t index_count = 0;
};

// Caches decode_mesh() + GPU-buffer-upload results keyed by ResourceId, so
// the same mesh asset is never re-resolved, re-decoded, or re-uploaded to
// the GPU more than once (issue #154) - mirrors atlas::audio::DecodeCache
// (issue #166) exactly for the resolve/decode half, extended with the one
// additional step atlas-render's own caches need beyond audio's CPU-only
// one: uploading the decoded vertex/index data to real SDL_GPUBuffer
// objects, which needs a live SDL_GPUDevice*. Both success and every
// failure mode are cached, not just successes - a resource that fails once
// (unresolved, resolution-failed, malformed, or a genuine GPU upload
// failure) must not be retried every frame it is referenced.
//
// Encapsulated (not rule of zero) for the same reason DecodeCache is: a
// cache entry must stay consistent with the registry/device it was built
// from for the lifetime of this object.
//
// GPU teardown is explicit (release()), never this class's own destructor -
// mirroring Sdl3TrianglePipeline's own explicit-teardown-before-device-
// destruction discipline (see that type's header, since removed as part of
// this same issue, and Sdl3FrameBackend's class doc comment): an
// automatically-invoked destructor here could run after Sdl3FrameBackend's
// own destroy() has already destroyed the SDL_GPUDevice this cache's
// buffers depend on, since C++ member destruction order runs after
// ~Sdl3FrameBackend()'s own body (which is what calls destroy()) completes.
// release() must be called explicitly, before the owning device is
// destroyed, and is idempotent - safe to call more than once.
class MeshUploadCache {
public:
    // registry and device must both outlive this cache - the same
    // "registry must outlive this cache" contract DecodeCache's own header
    // documents, extended to the device this cache uploads through.
    // type_name is the caller-supplied ResourceRegistry asset-type string
    // (the established convention, confirmed in
    // tests/atlas-resource/resource_registry_test.cpp, is "Mesh").
    MeshUploadCache(const resource::ResourceRegistry& registry,
                    std::string_view type_name,
                    SDL_GPUDevice* device);

    [[nodiscard]] const MeshUploadResult& get_or_upload(ResourceId id);

    // Releases every GPU buffer this cache has ever uploaded and clears the
    // cache - see this class's own doc comment for why this is not simply
    // ~MeshUploadCache(). Must be called before the SDL_GPUDevice this cache
    // was constructed with is destroyed.
    void release() noexcept;

private:
    const resource::ResourceRegistry* registry_;
    std::string type_name_;
    SDL_GPUDevice* device_;
    std::unordered_map<ResourceId, MeshUploadResult> cache_;
};

} // namespace atlas::render
