#include "atlas/render/frame_builder.hpp"

#include <optional>

namespace atlas::render {

Frame build_frame(std::span<const EntityRef> entities,
                  const runtime::PropertyStore<Transform>& transforms,
                  const runtime::PropertyStore<Renderable>& renderables,
                  const runtime::PropertyStore<CurrentAnimation>& current_animations,
                  const runtime::PropertyStore<AnimationPose>& poses,
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

        const auto current_animation = current_animations.get(entity);
        std::optional<AnimationPose> pose;
        if (current_animation.has_value()) {
            // This entity IS animated (something has set its
            // CurrentAnimation).
            const auto resolved_pose = poses.get(entity);
            if (!resolved_pose.has_value()) {
                // Animated, but no pose resolved for this tick yet (still
                // loading, or nothing has sampled one) - skip the whole
                // entity, following the existing "skip, never substitute"
                // convention. Do NOT draw an animated mesh with a
                // missing/default pose.
                continue;
            }
            pose = resolved_pose->get();
        }
        // else: entity was never animated at all - pose stays nullopt,
        // entity draws normally with just its static Transform, exactly
        // like today.

        frame.draw_commands.push_back(DrawCommand{
            .entity = entity,
            .transform = transform->get(),
            .mesh = renderable->get().mesh,
            .material = renderable->get().material,
            .pose = pose,
        });
    }

    return frame;
}

} // namespace atlas::render
