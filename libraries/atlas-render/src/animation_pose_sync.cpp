#include "atlas/render/animation_pose_sync.hpp"

#include "atlas/render/animation_sampler.hpp"
#include "atlas/render/skeleton_asset.hpp"

namespace atlas::render {

void sync_animation_poses(std::span<const EntityRef> entities,
                          const runtime::PropertyStore<CurrentAnimation>& current_animations,
                          const runtime::PropertyStore<AnimationPlaybackConfig>& playback_configs,
                          runtime::PropertyStore<AnimationClipTime>& clip_times,
                          runtime::PropertyStore<AnimationPose>& poses,
                          AnimationDecodeCache& animation_cache,
                          const resource::ResourceRegistry& registry,
                          std::string_view skeleton_type_name,
                          double delta_seconds) {
    for (const EntityRef entity : entities) {
        const auto current_animation = current_animations.get(entity);
        if (!current_animation.has_value()) {
            continue;
        }

        const auto previous_clip_time = clip_times.get(entity);
        const double elapsed_seconds =
            (previous_clip_time.has_value() ? previous_clip_time->get().elapsed_seconds : 0.0) +
            delta_seconds;
        clip_times.set(entity, AnimationClipTime{.elapsed_seconds = elapsed_seconds});

        const auto playback_config = playback_configs.get(entity);
        if (!playback_config.has_value()) {
            continue;
        }

        const AnimationDecodeResult& clip_result =
            animation_cache.get_or_decode(current_animation->get().clip);
        if (clip_result.status != AnimationDecodeCacheStatus::Ok) {
            continue;
        }

        const resource::Resolution skeleton_resolution =
            registry.resolve(skeleton_type_name, playback_config->get().skeleton);
        if (skeleton_resolution.status != resource::ResolutionStatus::Resolved) {
            continue;
        }

        const std::optional<SkeletonAsset> skeleton = decode_skeleton(skeleton_resolution.bytes);
        if (!skeleton.has_value()) {
            continue;
        }

        const std::optional<AnimationPose> pose = sample_animation_pose(
            clip_result.animation, *skeleton, elapsed_seconds, playback_config->get().loop);
        if (!pose.has_value()) {
            continue;
        }

        poses.set(entity, *pose);
    }
}

} // namespace atlas::render
