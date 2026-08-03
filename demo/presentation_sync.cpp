#include "presentation_sync.hpp"

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"

namespace atlas::demo {

void sync_transforms(std::span<const EntityRef> entities,
                     runtime::PropertyStore<movement::Position>& positions,
                     runtime::PropertyStore<render::Transform>& transforms) {
    for (const EntityRef entity : entities) {
        const auto position = positions.get(entity);
        if (!position.has_value()) {
            continue;
        }

        transforms.set(entity,
                       render::Transform{
                           .position = core::Vec3{.x = position->get().x, .y = position->get().y, .z = 0.0F},
                           .rotation = core::Quaternion{},
                           .scale = core::Vec3{1.0F, 1.0F, 1.0F},
                       });
    }
}

} // namespace atlas::demo
