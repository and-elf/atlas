#include "atlas/rcc/resource_table.hpp"

namespace atlas::rcc {

ResourceTable compile_resource_table(const std::vector<ResourceEntry>& entries) {
    ResourceTable table;
    table.reserve(entries.size());

    for (const auto& entry : entries) {
        const auto id = atlas::ResourceId::from_name(entry.name);

        std::optional<CompiledAnimationMetadata> animation;
        if (entry.animation.has_value()) {
            animation = CompiledAnimationMetadata{atlas::ResourceId::from_name(entry.animation->skeleton),
                                                  entry.animation->loop,
                                                  entry.animation->playback_rate};
        }

        table.emplace(id, CompiledResource{id, entry.name, entry.type, entry.path, std::move(animation)});
    }

    return table;
}

} // namespace atlas::rcc
