#pragma once

#include "atlas/physics/body.hpp"
#include "atlas/physics/body_id.hpp"

#include <concepts>
#include <optional>

namespace atlas::physics {

// The compile-time contract (spec §5: "checked like a C++ concept, never a
// runtime interface table or virtual dispatch lookup") every physics
// backend - real or null - must satisfy, mirroring atlas::audio::
// AudioBackend (libraries/atlas-audio/include/atlas/audio/audio_backend.hpp)
// and atlas::render::FrameBackend exactly. Which concrete type actually
// compiles into a given build is a configure-time CMake choice
// (ATLAS_PHYSICS_BACKEND, see libraries/atlas-physics/CMakeLists.txt),
// never a runtime factory or plugin lookup (spec §4).
//
// This is issue #177's own minimal slice - enough surface for a body to
// exist, be stepped forward, and be queried:
//
// - create_body()/destroy_body() manage a body's lifetime, returning/
//   consuming a BodyId (this library's own index+generation handle,
//   mirroring atlas::EntityRef).
// - step(delta_seconds) advances every live body forward by exactly the
//   caller-supplied timestep - never internally sourced from a clock. This
//   mirrors atlas::render::lerp(Transform...)'s own "alpha is caller-
//   supplied" discipline, and is a direct consequence of spec §4's
//   determinism rule that simulation-affecting code must never read
//   wall-clock time directly: unlike atlas-render/atlas-audio (presentation-
//   only, outside the determinism boundary), atlas-physics's output feeds
//   back into simulation state and so remains inside it
//   (docs/specification/24-non-goals.md).
// - body_state() reports a live body's current pose, or std::nullopt for a
//   destroyed or never-created id - never a stale/default value silently
//   substituted for "this body doesn't exist."
// - raycast(origin, direction, max_distance) (issue #180) reports the
//   closest body hit along a ray, or std::nullopt if nothing was hit within
//   max_distance. `direction` is a caller-supplied direction - not required
//   to be pre-normalized; a backend defensively normalizes it internally so
//   the ray's actual reach is always exactly max_distance regardless of the
//   magnitude the caller happened to pass (see JoltPhysicsBackend::raycast(),
//   src/jolt_physics_backend.cpp, for exactly how).
// - sweep(shape, from_position, from_rotation, to_position) (issue #180)
//   reports the closest body a caller-supplied shape would hit while
//   translating from from_position to to_position, keeping from_rotation
//   fixed throughout (a translation-only cast - e.g. moving a camera
//   collision volume from a pivot point to a desired position without also
//   rotating it in flight), or std::nullopt if nothing was hit.
// - Neither query sources anything internally (no clock, no hidden state) -
//   purely a function of the world's current body state and the
//   caller-supplied parameters, matching this contract's own step()
//   discipline. Both are §4 bit-exact-determinism-covered like every other
//   PhysicsBackend operation.
//
// Deliberately NOT in this contract yet, and why:
//
// - BodyCreateInfo does carry a shape now (issue #179, body.hpp's own
//   BodyShape variant - box/sphere/capsule/convex hull), but this concept
//   itself needed no change to add it: create_body() already took a whole
//   BodyCreateInfo by const reference, so a new field on that type is
//   invisible at this contract's own boundary.
// - No overlap/volume query, or any query shape beyond raycast + sweep -
//   issue #180's own explicit scope boundary; a real, undesigned follow-up
//   if a future capability needs one, not added speculatively here.
// - No velocity, mass, or force application - out of scope per issue #176's
//   own umbrella breakdown (#178/#179's "real rigid-body simulation"
//   sub-issues).
template <typename T>
concept PhysicsBackend = requires(T& backend,
                                  const BodyCreateInfo& create_info,
                                  BodyId body,
                                  float delta_seconds,
                                  core::Vec3 origin,
                                  core::Vec3 direction,
                                  float max_distance,
                                  const BodyShape& shape,
                                  core::Vec3 from_position,
                                  core::Quaternion from_rotation,
                                  core::Vec3 to_position) {
    { backend.create_body(create_info) } -> std::same_as<BodyId>;
    { backend.destroy_body(body) } -> std::same_as<void>;
    { backend.step(delta_seconds) } -> std::same_as<void>;
    { backend.body_state(body) } -> std::same_as<std::optional<BodyState>>;
    { backend.raycast(origin, direction, max_distance) } -> std::same_as<std::optional<HitResult>>;
    {
        backend.sweep(shape, from_position, from_rotation, to_position)
    } -> std::same_as<std::optional<HitResult>>;
};

} // namespace atlas::physics
