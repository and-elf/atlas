#pragma once

#include "atlas/render/transform.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace atlas::render {

// Sentinel `parent_index` meaning "this joint is a root" - a real index
// value (rather than, say, a separate `bool is_root` field) so a
// SkeletonAsset's parent-child relationship stays a single field per
// joint, mirroring how DecodedMesh's `indices` already uses plain u32
// values with no parallel validity flag.
inline constexpr std::uint32_t no_parent_joint = std::numeric_limits<std::uint32_t>::max();

// One joint in a decoded skeleton hierarchy: its parent's index into the
// same SkeletonAsset::joints array (or no_parent_joint for a root), plus its
// bind-pose Transform (transform.hpp - reused verbatim, no second parallel
// transform representation for skeletal data). A basic aggregate (rule of
// zero): no invariant beyond ordinary value semantics - the hierarchy
// invariant (every parent precedes its child) is decode_skeleton's own
// validation concern, not something Joint enforces of itself.
struct Joint {
    std::uint32_t parent_index = no_parent_joint;
    Transform bind_pose;
};

// The output of decode_skeleton: a joint hierarchy plus each joint's bind
// pose, decoded from this library's own minimal binary skeleton format
// (documented on decode_skeleton below). A basic aggregate (rule of zero):
// no invariant beyond ordinary value semantics.
struct SkeletonAsset {
    std::vector<Joint> joints;
};

// Decodes `bytes` (the kind of raw bytes atlas::resource::ResourceRegistry::
// resolve() produces) against this project's own minimal, hand-rolled
// skeleton format:
//
//   u32 joint_count
//   joint_count x {
//       u32 parent_index
//       float px, py, pz          -- bind-pose position
//       float qx, qy, qz, qw      -- bind-pose rotation (quaternion)
//       float sx, sy, sz          -- bind-pose scale
//   }                                                    -- 44 bytes each
//
// Integers/floats are read host-native (little-endian on every deployment
// target this project ships to - Debian 13 x86-64, macOS ARM, Windows
// x86-64 - stated explicitly rather than silently assumed, mirroring
// decode_mesh's own identical assumption, mesh_asset.hpp).
//
// A hand-rolled format rather than a third-party skeleton/animation
// importer - the same "a fixed header plus a flat array is trivial enough
// that a dependency wouldn't earn its keep" reasoning decode_mesh/
// decode_texture already document (mesh_asset.hpp, texture_asset.hpp).
//
// Required hierarchy invariant, checked for every joint: `parent_index`
// must be either no_parent_joint or strictly less than that joint's own
// index in the array. Requiring parents to precede their children in
// authoring order makes the hierarchy acyclic by construction - a corrupt
// or malicious blob can never make a consumer walk an infinite or
// out-of-bounds parent chain - and naturally forces joint 0 to always be a
// root (there is no earlier index it could legally point to), so "there
// must be at least one root" falls out of the same check rather than
// needing to be verified separately.
//
// Returns std::nullopt for any malformed or truncated input (too short a
// header, a declared joint count whose data would run past the end of
// `bytes`, or any joint violating the hierarchy invariant above) rather
// than throwing - matching decode_mesh/decode_texture's own convention: a
// corrupted asset is an ordinary runtime condition a host observes, not a
// build-time authoring error. A joint_count of zero is a well-formed,
// empty skeleton, not an error - mirroring decode_mesh's own stance that a
// mesh with zero vertices/indices is a valid, empty result.
[[nodiscard]] std::optional<SkeletonAsset> decode_skeleton(std::span<const std::byte> bytes);

} // namespace atlas::render
