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
//
// Deliberately NOT in this contract yet, and why:
//
// - No shape/geometry on BodyCreateInfo - #179's job, once a real backend
//   (#178) exists to define what shape primitives (box/sphere/capsule/
//   convex hull) are actually supported. Adding a shape field speculatively
//   now, with nothing but NullPhysicsBackend to test it against, would be
//   exactly the kind of undesigned surface this project's architecture
//   principles caution against.
// - No raycast/sweep query - genuinely blocked on #179/#180 needing real
//   shapes to test a query against first, not a deferred-by-choice gap:
//   a raycast against a shapeless NullPhysicsBackend body has no meaningful
//   answer to give, so the contract shape for it is left to #180 once
//   there's real geometry to validate it against.
// - No velocity, mass, or force application - out of scope per issue #176's
//   own umbrella breakdown (#178's "real rigid-body simulation" sub-issue).
template <typename T>
concept PhysicsBackend =
    requires(T& backend, const BodyCreateInfo& create_info, BodyId body, float delta_seconds) {
        { backend.create_body(create_info) } -> std::same_as<BodyId>;
        { backend.destroy_body(body) } -> std::same_as<void>;
        { backend.step(delta_seconds) } -> std::same_as<void>;
        { backend.body_state(body) } -> std::same_as<std::optional<BodyState>>;
    };

} // namespace atlas::physics
