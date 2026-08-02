#pragma once

#include "atlas/render/texture_asset.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atlas::render {

// Mirrors MeshUploadCacheStatus's own split exactly (see that header) -
// resolve/decode/upload distinguished the same way, for the same reason.
enum class TextureUploadCacheStatus : std::uint8_t {
    Ok,
    Unresolved,       // ResourceRegistry: type never registered, id absent, or a null id
    ResolutionFailed, // ResourceRegistry: the type was registered but its blob failed to load
    DecodeFailed,     // decode_texture(): malformed or truncated texture bytes
    UploadFailed,     // a real SDL_GPU texture create/map/upload call failed
};

// texture/width/height are only meaningful when status == Ok, mirroring
// MeshUploadResult's own convention. A texture that decodes to zero width
// or height (decode_texture's own well-formed "empty texture" case) is
// still Ok, with texture left null - Sdl3FrameBackend::submit() skips
// issuing a draw call for it, the same "skip, never substitute" handling
// as an outright failure, just for a different underlying reason.
struct TextureUploadResult {
    TextureUploadCacheStatus status = TextureUploadCacheStatus::Unresolved;
    SDL_GPUTexture* texture = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Caches decode_texture() + GPU-texture-upload results keyed by ResourceId,
// so the same texture asset is never re-resolved, re-decoded, or
// re-uploaded to the GPU more than once (issue #154) - mirrors
// MeshUploadCache exactly (see that header for the full design reasoning,
// itself mirroring atlas::audio::DecodeCache, issue #166), just uploading a
// decoded RGBA8 pixel buffer to a real SDL_GPUTexture instead of a
// vertex/index buffer pair. Both success and every failure mode are
// cached, not just successes.
//
// Encapsulated (not rule of zero) for the same reason MeshUploadCache is.
//
// GPU teardown is explicit (release()), never this class's own destructor -
// see MeshUploadCache's own doc comment for why (the same
// Sdl3FrameBackend::destroy() device-lifetime-ordering hazard applies here
// too). Idempotent - safe to call more than once.
class TextureUploadCache {
public:
    // registry and device must both outlive this cache. type_name is the
    // caller-supplied ResourceRegistry asset-type string (the established
    // convention, confirmed in tests/atlas-resource/resource_registry_test.cpp,
    // is "Texture").
    TextureUploadCache(const resource::ResourceRegistry& registry,
                       std::string_view type_name,
                       SDL_GPUDevice* device);

    [[nodiscard]] const TextureUploadResult& get_or_upload(ResourceId id);

    // Releases every GPU texture this cache has ever uploaded and clears
    // the cache. Must be called before the SDL_GPUDevice this cache was
    // constructed with is destroyed.
    void release() noexcept;

private:
    const resource::ResourceRegistry* registry_;
    std::string type_name_;
    SDL_GPUDevice* device_;
    std::unordered_map<ResourceId, TextureUploadResult> cache_;
};

} // namespace atlas::render
