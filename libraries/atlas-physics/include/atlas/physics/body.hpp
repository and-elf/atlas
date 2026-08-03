#pragma once

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"

#include <cstdint>

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

// The information PhysicsBackend::create_body() needs to bring a new body
// into existence. Deliberately no shape/geometry field yet - issue #179's
// job once a real backend (#178) exists to define what shape primitives are
// actually supported (box/sphere/capsule/convex hull, per #179's own scope).
// This round proves the body-exists/step/query mechanism only, so a body
// has a motion type and an initial pose and nothing else - a real backend
// would refuse to actually simulate collision against a shapeless body
// (#178's problem to solve, not this contract's). A basic aggregate (rule
// of zero): no invariant beyond ordinary value semantics.
struct BodyCreateInfo {
    BodyMotionType motion_type = BodyMotionType::Static;
    core::Vec3 position;
    core::Quaternion rotation;
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
