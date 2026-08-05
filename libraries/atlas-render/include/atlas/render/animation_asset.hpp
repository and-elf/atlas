#pragma once

#include "atlas/render/transform.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace atlas::render {

// One sampled instant in a decoded animation clip: a time (seconds, from
// clip start) plus one Transform per joint, in the same joint-index order
// as the SkeletonAsset (skeleton_asset.hpp) this clip is meant to be
// sampled against - joint correspondence is positional (index N here means
// the same joint as index N in that SkeletonAsset), the same convention
// decode_skeleton's own Joint::parent_index already relies on for its
// hierarchy. A basic aggregate (rule of zero): no invariant beyond ordinary
// value semantics.
struct AnimationKeyframe {
    float time = 0.0F;
    std::vector<Transform> joint_transforms; // size == DecodedAnimation::joint_count
};

// The output of decode_animation: a clip's full set of keyframes, decoded
// from this library's own minimal binary animation format (documented on
// decode_animation below). A basic aggregate (rule of zero): no invariant
// beyond ordinary value semantics - unlike decode_skeleton's SkeletonAsset,
// there is no hierarchy to validate here, only flat per-keyframe joint
// transform data.
struct DecodedAnimation {
    std::uint32_t joint_count = 0;
    std::vector<AnimationKeyframe> keyframes;
};

// Decodes `bytes` (the kind of raw bytes atlas::resource::ResourceRegistry::
// resolve() produces) against this project's own minimal, hand-rolled
// animation clip format:
//
//   u32 joint_count
//   u32 keyframe_count
//   keyframe_count x {
//       float time
//       joint_count x { float px, py, pz; float qx, qy, qz, qw; float sx, sy, sz }  -- one Transform, 40
//       bytes
//   }
//
// Integers/floats are read host-native (little-endian on every deployment
// target this project ships to - Debian 13 x86-64, macOS ARM, Windows
// x86-64 - stated explicitly rather than silently assumed, mirroring
// decode_mesh/decode_skeleton's own identical assumption, mesh_asset.hpp/
// skeleton_asset.hpp).
//
// A hand-rolled format rather than a third-party animation curve importer
// (FBX/glTF) - mirroring decode_mesh/decode_skeleton's own "a fixed header
// plus flat arrays is trivial enough that a dependency wouldn't earn its
// keep" reasoning verbatim; real animation-curve import is explicitly out
// of scope (see issue #229's own non-goals, matching #45/#46's own
// content-pipeline boundary, spec §24).
//
// joint_count == 0 and/or keyframe_count == 0 both decode successfully to
// an empty/well-formed result, never an error - mirroring decode_mesh's own
// zero-vertex stance and decode_skeleton's own zero-joint stance (both
// already in this codebase). Malformed/truncated bytes - too short a
// header, or declared joint/keyframe counts whose data would run past the
// end of `bytes` - return std::nullopt rather than throwing, matching
// decode_mesh/decode_skeleton/decode_texture's own convention: a corrupted
// asset is an ordinary runtime condition a host observes, not a build-time
// authoring error.
//
// The truncation check is overflow-safe rather than multiplying
// keyframe_count * joint_count * 40 directly: joint_count and
// keyframe_count are each independently-declared u32 values (unlike
// decode_mesh's vertex/index counts, which only ever multiply against a
// fixed per-element size and never against each other), so their product
// can overflow std::size_t for adversarial input the same way
// decode_texture's width * height can - see that function's own doc
// comment/implementation for the identical division-based guard mirrored
// here.
[[nodiscard]] std::optional<DecodedAnimation> decode_animation(std::span<const std::byte> bytes);

} // namespace atlas::render
