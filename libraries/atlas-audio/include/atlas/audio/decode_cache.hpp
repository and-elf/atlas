#pragma once

#include "atlas/audio/wav_decoder.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace atlas::audio {

// Distinguishes every way a request can fail, mirroring
// atlas::resource::ResolutionStatus's own split (Unresolved vs.
// ResolutionFailed) plus decode_wav()'s own split (Malformed vs.
// UnsupportedFormat) - collapsing these into one generic "failed" status
// would throw away exactly the diagnostic distinction each layer already
// took care to report.
enum class DecodeCacheStatus : std::uint8_t {
    Ok,
    Unresolved,              // ResourceRegistry: type never registered, id absent, or a null id
    ResolutionFailed,        // ResourceRegistry: the type was registered but its blob failed to load
    DecodeMalformed,         // decode_wav(): not a well-formed WAV container
    DecodeUnsupportedFormat, // decode_wav(): a well-formed WAV, just not the canonical PCM format
};

// clip is only meaningful when status == Ok, matching
// atlas::resource::Resolution's own "payload meaningful only on success"
// convention.
struct DecodeCacheResult {
    DecodeCacheStatus status = DecodeCacheStatus::Unresolved;
    DecodedClip clip;
};

// Caches decode_wav() results keyed by ResourceId, so the same asset's WAV
// bytes are never resolved or decoded more than once (issue #164) -
// resolving and decoding on every mix would be wasteful, and
// ResourceRegistry's own blobs are loaded once at construction and never
// change underneath it, so a request that fails once will fail identically
// forever; both outcomes are cached, not just successes.
//
// Encapsulated (not rule of zero): protects a real invariant, the same
// reasoning ResourceRegistry's own header documents for its own class shape
// - a cache entry must stay consistent with the registry it was built from
// for the lifetime of this object.
//
// Not thread-safe, deliberately: every call in this round's design
// originates from the sim thread alone (MiniaudioBackend::submit()/
// trigger(), issue #55), never from the audio callback thread itself - a
// cache safe to call from the audio thread too is a different, harder
// problem this class does not attempt to solve (see issue #164's own
// scoping).
//
// No eviction policy - flagged as unresolved, not silently assumed fine
// (per #55/#164's own scoping discussion). This cache grows unboundedly for
// the lifetime of the process.
class DecodeCache {
public:
    // registry must outlive this cache. type_name is the asset-type string
    // ResourceRegistry itself was configured with for audio assets (e.g.
    // "Sound") - a caller-supplied, not hard-coded, convention, since naming
    // resource types is an asset-pipeline decision, not something this
    // foundational library should assume (spec §2, Mechanism Over Meaning).
    explicit DecodeCache(const resource::ResourceRegistry& registry, std::string_view type_name);

    [[nodiscard]] const DecodeCacheResult& get_or_decode(ResourceId id);

private:
    const resource::ResourceRegistry* registry_;
    std::string type_name_;
    std::unordered_map<ResourceId, DecodeCacheResult> cache_;
};

} // namespace atlas::audio
