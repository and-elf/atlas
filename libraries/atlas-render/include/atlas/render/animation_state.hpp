#pragma once

#include "atlas/render/transform.hpp"
#include "atlas/resource/resource_id.hpp"

#include <vector>

namespace atlas::render {

// Which animation clip an entity is currently playing, named by stable
// resource identity (spec §3, Resource) rather than a hard-coded path or an
// in-memory clip handle - the same identity-only stance Renderable already
// takes for mesh/material (renderable.hpp). Ordinary composed simulation
// state (spec §20): capability-authored, deterministic, replicated exactly
// like MovementSpeed - composed via Priority Override
// (runtime::resolve_priority_override, property_composition.hpp), the same
// strategy spec §20's own worked example ("AnimationState: Stunned >
// Weapon > Default") describes. This issue defines the type only; no
// capability contributing to it exists yet - that's separate, future work.
// A basic aggregate (rule of zero): no invariant beyond ordinary value
// semantics. A default-constructed CurrentAnimation holds a null clip
// ResourceId - build_frame (frame_builder.hpp) treats *presence* of an
// entry here (regardless of whether the clip itself resolves) as "this
// entity is animated", so a capability contributing a genuinely unresolved
// clip is still a distinct, deliberate case from never contributing at all.
struct CurrentAnimation {
    ResourceId clip;
};

// A runtime speed modifier applied to whichever clip CurrentAnimation
// names (spec §20), composed via Multiplicative
// (runtime::resolve_multiplicative) - the same strategy MovementSpeed
// already uses, with `1.0F` as the strategy's own required identity value
// (resolve_multiplicative's own doc comment: "callers must pass the
// property's actual declared base... or the result silently loses the
// base entirely"). The declared base this multiplies against is
// #45's manifest-authored `playback_rate`, not anything this library
// stores itself. Declared vocabulary only - nothing in this issue's own
// code reads or uses this value; it exists for a future capability (haste/
// slow effects) and for issue #229 (blocked on this one) to consume. A
// basic aggregate (rule of zero): no invariant beyond ordinary value
// semantics.
struct AnimationPlaybackRate {
    float value = 1.0F;
};

// The resolved, blended per-joint transform set actually used for
// rendering one animated entity this tick - one Transform per joint, same
// order and count as the referenced SkeletonAsset's joints
// (skeleton_asset.hpp, decode_skeleton). Composed via Weighted Composition
// (runtime::resolve_weighted_composition) in the general case (spec §20's
// own worked example: "AnimationPose: 70% Walk, 30% Run"), but for this
// issue's own scope build_frame (frame_builder.hpp) never computes one
// itself - it only plumbs an already-resolved value stored here through to
// DrawCommand::pose. Real keyframe decoding/interpolation (a clip ->
// AnimationPose sampling function) is issue #229's job, a sibling issue
// blocked on this one, not implemented here. A basic aggregate (rule of
// zero): no invariant beyond ordinary value semantics - joint_transforms
// matching its referenced skeleton's joint count/order is a producer
// obligation this type does not itself enforce, the same stance Renderable
// takes toward its own mesh/material actually resolving.
struct AnimationPose {
    std::vector<Transform> joint_transforms;
};

} // namespace atlas::render
