#pragma once

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace atlas::physics {

// Whether a body is simulated (moved/rotated by forces and collision
// response every step) or fixed in place forever - the minimal distinction
// every rigid-body engine's own body-creation API needs before any real
// simulation exists to drive it (issue #177 proves only "a body exists, is
// stepped, and is queried" - #178/#179 are where forces, mass, and real
// collision response actually get simulated). Kinematic bodies (moved
// directly by a capability rather than either forces or nothing at all) are
// deliberately not a third option yet - nothing in this round's scope needs
// the distinction, and adding it speculatively ahead of a real backend to
// test it against would be exactly the kind of undesigned surface CLAUDE.md's
// architecture principles caution against.
enum class BodyMotionType : std::uint8_t { Static, Dynamic };

// A body's collision shape (issue #179) - deliberately limited to the same
// small set of primitive kinds Jolt supports natively (box, sphere, capsule,
// convex hull) rather than inventing something exotic (a custom mesh-collider
// format, novel primitive types), but expressed as a plain, backend-agnostic
// Atlas-defined data type here in the contract layer (body.hpp), never as
// Jolt's own C++ shape types (`JPH::Shape` et al.) directly: body.hpp is
// shared by NullPhysicsBackend (issue #177, zero third-party dependencies,
// mirroring NullFrameBackend/NullAudioBackend's own "compiles everywhere"
// role) and JoltPhysicsBackend alike, so it cannot depend on Jolt's own
// headers without breaking that. JoltPhysicsBackend
// (src/jolt_physics_backend.cpp) converts a BodyShape into a real
// JPH::BoxShape/JPH::SphereShape/JPH::CapsuleShape/JPH::ConvexHullShape
// internally; NullPhysicsBackend simply stores/ignores it exactly the way it
// already ignores everything else about a body's state beyond
// position/rotation (its own doc comment, null_physics_backend.hpp).
//
// Each alternative is a basic aggregate (rule of zero): no invariant beyond
// ordinary value semantics - a BoxShape with a negative half-extent, or a
// ConvexHullShape with too few points to form a hull, is a caller/backend
// concern (JoltPhysicsBackend::create_body() throws std::runtime_error for a
// convex hull Jolt's own ConvexHullShapeSettings::Create() rejects), not
// something this plain data type constructs against.
struct BoxShape {
    core::Vec3 half_extents = {.x = 0.5F, .y = 0.5F, .z = 0.5F};
};

struct SphereShape {
    float radius = 0.5F;
};

struct CapsuleShape {
    float half_height = 0.5F;
    float radius = 0.5F;
};

// `points` need not already be the hull's own vertices - interior points and
// points on faces/edges are fine (a real backend's convex-hull construction,
// e.g. JPH::ConvexHullShapeSettings, discards them) - see this project's
// own README for the exact backend behavior on a degenerate point set.
struct ConvexHullShape {
    std::vector<core::Vec3> points;
};

// The backend-agnostic shape vocabulary a BodyCreateInfo carries - exactly
// the four primitive kinds documented above, no more (see this file's own
// top-of-struct doc comment for why, and this library's README "Open
// questions" for what a future shape kind - e.g. trimesh/heightfield - would
// need to be a deliberate, designed follow-up rather than added
// speculatively here).
using BodyShape = std::variant<BoxShape, SphereShape, CapsuleShape, ConvexHullShape>;

// The information PhysicsBackend::create_body() needs to bring a new body
// into existence. `shape` defaults to a 0.5m-radius SphereShape - the exact
// placeholder radius issue #178's JoltPhysicsBackend hardcoded for every body
// before this field existed - so every pre-#179 call site (BodyCreateInfo{...}
// with no shape specified) keeps compiling and behaving unchanged. A basic
// aggregate (rule of zero): no invariant beyond ordinary value semantics.
struct BodyCreateInfo {
    BodyMotionType motion_type = BodyMotionType::Static;
    core::Vec3 position;
    core::Quaternion rotation;
    BodyShape shape = SphereShape{.radius = 0.5F};
};

// A body's resolved pose for the tick it was queried at - the only state a
// caller can observe about a body through this contract today (no
// linear/angular velocity, no shape, no material). A basic aggregate (rule
// of zero): no invariant beyond ordinary value semantics.
struct BodyState {
    core::Vec3 position;
    core::Quaternion rotation;
};

} // namespace atlas::physics
