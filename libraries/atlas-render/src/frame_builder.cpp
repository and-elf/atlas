#include "atlas/render/frame_builder.hpp"

namespace atlas::render {

Frame build_frame(std::span<const EntityRef> entities,
                  const runtime::PropertyStore<Transform>& transforms,
                  const runtime::PropertyStore<Renderable>& renderables,
                  core::Time tick) {
    Frame frame{.tick = tick, .draw_commands = {}};
    frame.draw_commands.reserve(entities.size());

    for (const auto& entity : entities) {
        const auto transform = transforms.get(entity);
        if (!transform.has_value()) {
            continue;
        }

        const auto renderable = renderables.get(entity);
        if (!renderable.has_value()) {
            continue;
        }

        if (renderable->get().mesh.is_null() || renderable->get().material.is_null()) {
            continue;
        }

        frame.draw_commands.push_back(DrawCommand{
            .entity = entity,
            .transform = transform->get(),
            .mesh = renderable->get().mesh,
            .material = renderable->get().material,
        });
    }

    return frame;
}

} // namespace atlas::render
