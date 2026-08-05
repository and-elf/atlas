#pragma once

#include "atlas/render/animation_asset.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace atlas::render {

// Distinguishes every way resolving+decoding an animation clip can fail,
// mirroring atlas::audio::DecodeCacheStatus's own split (issue #164, this
// type's direct template) collapsed to the one decode failure mode
// decode_animation actually has: unlike decode_wav (Malformed vs.
// UnsupportedFormat), decode_animation returns a single std::nullopt for
// any malformed/truncated input, so there is only one decode-failure case
// to distinguish - the same shape MeshUploadCacheStatus already uses for
// decode_mesh (mesh_upload_cache.hpp), which returns std::optional the same
// way.
enum class AnimationDecodeCacheStatus : std::uint8_t {
    Ok,
    Unresolved,       // ResourceRegistry: type never registered, id absent, or a null id
    ResolutionFailed, // ResourceRegistry: the type was registered but its blob failed to load
    DecodeFailed,     // decode_animation(): malformed or truncated clip bytes
};

// animation is only meaningful when status == Ok, matching
// atlas::audio::DecodeCacheResult's own "payload meaningful only on
// success" convention.
struct AnimationDecodeResult {
    AnimationDecodeCacheStatus status = AnimationDecodeCacheStatus::Unresolved;
    DecodedAnimation animation;
};

// Caches decode_animation() results keyed by ResourceId, so the same clip
// asset's bytes are never resolved or decoded more than once - mirrors
// atlas::audio::DecodeCache (issue #164) exactly, CPU-only with no GPU
// upload step: unlike MeshUploadCache/TextureUploadCache, a clip's decoded
// keyframes feed a sampling function directly (sample_animation_pose,
// issue #229's Phase 2) rather than a static GPU buffer, so there is no
// SDL_GPUDevice-dependent release() step to mirror from those two caches -
// this class's shape stays exactly DecodeCache's own.
//
// Encapsulated (not rule of zero) for the same reason DecodeCache is: a
// cache entry must stay consistent with the registry it was built from for
// the lifetime of this object.
//
// Not thread-safe, deliberately - the same single-sim-thread-caller scoping
// DecodeCache's own header documents; a cache safe to call from a render
// thread too is a different, harder problem this class does not attempt to
// solve.
//
// No eviction policy - flagged as unresolved, not silently assumed fine,
// mirroring DecodeCache's own identical scoping note. This cache grows
// unboundedly for the lifetime of the process.
class AnimationDecodeCache {
public:
    // registry must outlive this cache. type_name is the asset-type string
    // ResourceRegistry itself was configured with for animation clip assets
    // (e.g. "Animation", matching #45's own atlas-rcc manifest naming) - a
    // caller-supplied, not hard-coded, convention, the same reasoning
    // DecodeCache's own header gives for its own type_name parameter.
    explicit AnimationDecodeCache(const resource::ResourceRegistry& registry, std::string_view type_name);

    [[nodiscard]] const AnimationDecodeResult& get_or_decode(ResourceId id);

private:
    const resource::ResourceRegistry* registry_;
    std::string type_name_;
    std::unordered_map<ResourceId, AnimationDecodeResult> cache_;
};

} // namespace atlas::render
