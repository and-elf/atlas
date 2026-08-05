#include "atlas/render/animation_sampler.hpp"

#include <cmath>

namespace atlas::render {

namespace {

// keyframes assumed non-empty and sorted by non-decreasing time - both
// this function's own callers already guarantee (sample_animation_pose
// checks emptiness itself before calling this; sort order is
// decode_animation's own producer obligation, see animation_asset.hpp).
double resolve_playback_time(const DecodedAnimation& animation, double elapsed_seconds, bool loop) {
    const double clip_length = static_cast<double>(animation.keyframes.back().time);

    if (loop) {
        if (clip_length <= 0.0) {
            return 0.0;
        }
        const double wrapped = std::fmod(elapsed_seconds, clip_length);
        return (wrapped < 0.0) ? wrapped + clip_length : wrapped;
    }

    if (elapsed_seconds < 0.0) {
        return 0.0;
    }
    if (elapsed_seconds > clip_length) {
        return clip_length;
    }
    return elapsed_seconds;
}

} // namespace

std::optional<AnimationPose> sample_animation_pose(const DecodedAnimation& animation,
                                                   const SkeletonAsset& skeleton,
                                                   double elapsed_seconds,
                                                   bool loop) {
    if (animation.joint_count != skeleton.joints.size()) {
        return std::nullopt;
    }
    if (animation.keyframes.empty()) {
        return std::nullopt;
    }

    if (animation.keyframes.size() == 1) {
        return AnimationPose{.joint_transforms = animation.keyframes.front().joint_transforms};
    }

    const double playback_time = resolve_playback_time(animation, elapsed_seconds, loop);

    // A linear scan for the smallest upper_index such that either it is the
    // last keyframe, or playback_time doesn't yet exceed it - keyframes.size()
    // is at least 2 here, so upper_index starts at 1 and the loop's own upper
    // bound (size() - 1) is always reachable.
    std::size_t upper_index = 1;
    while (upper_index < animation.keyframes.size() - 1 &&
           playback_time > static_cast<double>(animation.keyframes[upper_index].time)) {
        ++upper_index;
    }
    const std::size_t lower_index = upper_index - 1;

    const AnimationKeyframe& lower = animation.keyframes[lower_index];
    const AnimationKeyframe& upper = animation.keyframes[upper_index];

    const double span = static_cast<double>(upper.time) - static_cast<double>(lower.time);
    const double alpha = (span > 0.0) ? (playback_time - static_cast<double>(lower.time)) / span : 0.0;

    AnimationPose pose;
    pose.joint_transforms.reserve(animation.joint_count);
    for (std::uint32_t joint_index = 0; joint_index < animation.joint_count; ++joint_index) {
        pose.joint_transforms.push_back(
            lerp(lower.joint_transforms[joint_index], upper.joint_transforms[joint_index], alpha));
    }

    return pose;
}

} // namespace atlas::render
