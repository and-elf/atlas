#include "atlas/audio/decode_cache.hpp"

namespace atlas::audio {

DecodeCache::DecodeCache(const resource::ResourceRegistry& registry, std::string_view type_name)
    : registry_(&registry), type_name_(type_name) {}

const DecodeCacheResult& DecodeCache::get_or_decode(ResourceId id) {
    const auto cached_it = cache_.find(id);
    if (cached_it != cache_.end()) {
        return cached_it->second;
    }

    const resource::Resolution resolution = registry_->resolve(type_name_, id);

    DecodeCacheResult result;
    switch (resolution.status) {
        case resource::ResolutionStatus::Unresolved:
            result.status = DecodeCacheStatus::Unresolved;
            break;
        case resource::ResolutionStatus::ResolutionFailed:
            result.status = DecodeCacheStatus::ResolutionFailed;
            break;
        case resource::ResolutionStatus::Resolved: {
            const WavDecodeResult decoded = decode_wav(resolution.bytes);
            switch (decoded.status) {
                case WavDecodeStatus::Ok:
                    result.status = DecodeCacheStatus::Ok;
                    result.clip = decoded.clip;
                    break;
                case WavDecodeStatus::Malformed:
                    result.status = DecodeCacheStatus::DecodeMalformed;
                    break;
                case WavDecodeStatus::UnsupportedFormat:
                    result.status = DecodeCacheStatus::DecodeUnsupportedFormat;
                    break;
            }
            break;
        }
    }

    return cache_.emplace(id, std::move(result)).first->second;
}

} // namespace atlas::audio
