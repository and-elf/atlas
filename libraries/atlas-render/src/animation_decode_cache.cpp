#include "atlas/render/animation_decode_cache.hpp"

namespace atlas::render {

AnimationDecodeCache::AnimationDecodeCache(const resource::ResourceRegistry& registry,
                                           std::string_view type_name)
    : registry_(&registry), type_name_(type_name) {}

const AnimationDecodeResult& AnimationDecodeCache::get_or_decode(ResourceId id) {
    const auto cached_it = cache_.find(id);
    if (cached_it != cache_.end()) {
        return cached_it->second;
    }

    const resource::Resolution resolution = registry_->resolve(type_name_, id);

    AnimationDecodeResult result;
    switch (resolution.status) {
        case resource::ResolutionStatus::Unresolved:
            result.status = AnimationDecodeCacheStatus::Unresolved;
            break;
        case resource::ResolutionStatus::ResolutionFailed:
            result.status = AnimationDecodeCacheStatus::ResolutionFailed;
            break;
        case resource::ResolutionStatus::Resolved: {
            std::optional<DecodedAnimation> decoded = decode_animation(resolution.bytes);
            if (decoded.has_value()) {
                result.status = AnimationDecodeCacheStatus::Ok;
                result.animation = std::move(decoded).value();
            } else {
                result.status = AnimationDecodeCacheStatus::DecodeFailed;
            }
            break;
        }
    }

    return cache_.emplace(id, std::move(result)).first->second;
}

} // namespace atlas::render
