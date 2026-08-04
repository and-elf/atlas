#pragma once

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"
#include "atlas/physics/body.hpp"
#include "atlas/physics/physics_backend.hpp"

#include <optional>

namespace atlas::demo {

// Issue #182: the actual motivating use case for the atlas-physics arc
// (#176) - a follow/orbit camera that doesn't clip through walls.
// Deliberately lives here, not inside atlas-render or atlas-physics: #182's
// own issue discussion confirmed no spec/CLAUDE.md rule literally forbids an
// atlas-render -> atlas-physics dependency (unlike the atlas-render/
// atlas-input sibling-library rule atlas-windowing exists to mediate), but
// the same avoidable-coupling reasoning still applies - most atlas-render
// consumers never touch physics and vice versa. This helper takes a
// PhysicsBackend and plain position data as ordinary function parameters
// (dependency injection at the composing layer) rather than being baked
// into either library it connects - the same place demo's own
// capability-to-capability glue already lives (e.g. presentation_sync.hpp's
// sync_transforms, bridging movement::Position into render::Transform).
//
// Sweeps a small sphere (see "Shape choice" below) from `pivot` (the
// camera's target/orbit point - typically the entity the camera follows)
// toward `desired_position` (wherever camera-control logic - orbit/follow
// input handling, explicitly out of this issue's scope, §2 - wants the
// camera to sit this tick). If the sweep hits anything before reaching
// desired_position, the returned position is clamped to the hit point
// (standard third-person-camera-collision technique); otherwise
// desired_position is returned unchanged.
//
// Shape choice: a sphere, not a capsule. PhysicsBackend::sweep() takes a
// `from_rotation` held fixed throughout the cast (its own doc comment) - a
// sphere is rotationally symmetric, so that fixed rotation is always
// physically meaningless for it, sidestepping any question of which
// orientation a camera's collision volume should be swept at. A capsule
// would need a real answer to that question (e.g. always upright, matching
// the camera's own up vector) this round's scope doesn't need to solve.
//
// Static-vs-dynamic decision: this sweeps against whatever
// PhysicsBackend::sweep() itself reports, which - per that contract's own
// scope (issue #180) - does not distinguish static from dynamic bodies at
// all (no broadphase-layer/motion-type filter parameter exists on the
// contract today). So the camera stops at *any* body's surface, static
// geometry or a dynamic prop/NPC alike - not a deliberate "always stops for
// dynamic bodies too" design choice, just the honest consequence of
// sweep()'s current, unfiltered surface. Letting the camera pass through a
// moving dynamic body instead would need a new, layer-aware sweep overload
// on PhysicsBackend itself - a real, flagged follow-up, not solved here.
template <physics::PhysicsBackend Backend>
[[nodiscard]] core::Vec3 resolve_camera_collision(Backend& backend,
                                                  const core::Vec3& pivot,
                                                  const core::Vec3& desired_position,
                                                  float collision_radius = 0.2F) {
    const std::optional<physics::HitResult> hit = backend.sweep(
        physics::SphereShape{.radius = collision_radius}, pivot, core::Quaternion{}, desired_position);
    return hit ? hit->point : desired_position;
}

} // namespace atlas::demo
