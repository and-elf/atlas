#pragma once

#include "atlas/render/animation_asset.hpp"
#include "atlas/render/animation_state.hpp"
#include "atlas/render/skeleton_asset.hpp"

#include <optional>

namespace atlas::render {

// Samples/interpolates one DecodedAnimation clip (animation_asset.hpp)
// against a SkeletonAsset's joint hierarchy (skeleton_asset.hpp) at a given
// point in playback time, producing the resolved per-joint AnimationPose
// build_frame (frame_builder.hpp, issue #46) plumbs into DrawCommand::pose.
//
// Returns std::nullopt (never an out-of-bounds read or a silently-wrong
// pose) when:
// - `animation.joint_count != skeleton.joints.size()` - a mismatched
//   skeleton, the same "producer obligation, not enforced by the type
//   itself" stance AnimationPose's own doc comment already takes toward its
//   joint_transforms matching some skeleton's joint count; this function is
//   the one place that actually checks it before producing a pose;
// - `animation.keyframes.empty()` - nothing to sample.
//
// If `animation.keyframes` holds exactly one keyframe, that keyframe's pose
// is returned directly - verbatim, never interpolated - regardless of
// `elapsed_seconds`: there is no second keyframe to interpolate toward, and
// clamping/wrapping a single-keyframe clip's one timestamp against itself
// would be a no-op that only obscures the real behavior (return exactly
// what's there).
//
// Otherwise, `elapsed_seconds` is first resolved against the clip's total
// length (its last keyframe's `time` - keyframes are assumed already sorted
// by non-decreasing `time`, a decode_animation producer obligation this
// function does not itself re-validate, the same stance it takes toward
// joint-count matching above): if `loop` is true, `elapsed_seconds` wraps
// modulo that length (std::fmod, folded back into [0, length) for a
// negative input); if `loop` is false, it clamps to [0, length] instead, so
// playback holds on the final pose once finished rather than running out of
// range. The two keyframes bracketing the resolved time are then located by
// a linear scan (this format's keyframe counts are small - tens, not
// thousands - so this is not a hot path worth a binary search over), and
// every joint's Transform is interpolated between them via transform.hpp's
// existing `lerp(const Transform&, const Transform&, double)` - reused
// verbatim (composes position lerp + nlerp rotation + scale lerp), never
// reimplemented here.
[[nodiscard]] std::optional<AnimationPose> sample_animation_pose(const DecodedAnimation& animation,
                                                                 const SkeletonAsset& skeleton,
                                                                 double elapsed_seconds,
                                                                 bool loop);

} // namespace atlas::render
