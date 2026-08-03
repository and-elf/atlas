#pragma once

#include "atlas/physics/body.hpp"
#include "atlas/physics/body_id.hpp"
#include "atlas/physics/physics_backend.hpp"

#include <optional>
#include <vector>

namespace atlas::physics {

// The always-buildable atlas::physics::PhysicsBackend (issue #177): stores
// each created body's BodyState in a plain, monotonically-growing vector
// and does nothing else - no forces, no collision, no shape simulation.
// create_body() accepts BodyCreateInfo::shape (issue #179) exactly like
// every other field it doesn't act on - additional data this backend never
// inspects, matching its own "does nothing with body state beyond echoing
// position/rotation" precedent. Exists so the mechanism up to the backend
// boundary (this contract itself) stays fully buildable and testable with
// zero third-party dependencies, independent of whichever real backend
// (#178/#179) a given build opts into via ATLAS_PHYSICS_BACKEND.
//
// Unlike NullFrameBackend/NullAudioBackend (which track nothing, or one
// scalar), body_state() must answer honestly for a specific body id, so
// this backend needs real per-body storage: `bodies`, indexed by
// BodyId::index, where a std::nullopt entry marks a destroyed slot and an
// out-of-range index marks one that was never created.
//
// create_body() allocates a new BodyId via a monotonically incrementing
// index (generation always 0) - no free-list, no index reuse. A real
// backend's own body-handle reuse strategy (e.g. #178/#179's Jolt-backed
// one, which may recycle a destroyed body's slot and bump its generation
// the same way atlas::entity::EntityRegistry does for EntityRef) is that
// backend's own concern, not something this contract mandates.
//
// destroy_body() clears the stored state for a valid index so a subsequent
// body_state() call reports std::nullopt, but never shrinks `bodies` or
// reuses the slot - every previously issued BodyId::index stays stable for
// the lifetime of this backend instance.
//
// body_state() does not check BodyId::generation. Since this backend never
// reuses an index, every index it ever hands out corresponds to exactly one
// generation (0) for its entire lifetime - a "stale generation" case (the
// reason EntityRegistry-style handles check it at all) cannot arise here
// structurally, so checking it would only add a branch with nothing behind
// it to reject. A real backend that does reuse indices must check
// generation itself.
//
// step() is a genuine no-op: it does nothing with any body's stored state,
// proving nothing is silently integrated/moved - mirroring
// NullAudioBackend's "does nothing with a resolved cue list" precedent
// applied here to "does nothing with body state."
//
// A basic aggregate (rule of zero): `bodies` is plain public state with no
// invariant beyond ordinary vector semantics - matches NullFrameBackend's
// own precedent of public fields plus plain member functions, rather than
// encapsulation hiding a vector with nothing extra to protect.
struct NullPhysicsBackend {
    std::vector<std::optional<BodyState>> bodies;

    [[nodiscard]] BodyId create_body(const BodyCreateInfo& create_info) {
        const auto index = static_cast<BodyId::IndexType>(bodies.size());
        bodies.emplace_back(BodyState{.position = create_info.position, .rotation = create_info.rotation});
        return BodyId{.index = index, .generation = 0};
    }

    void destroy_body(BodyId body) noexcept {
        if (body.index < bodies.size()) {
            bodies[body.index].reset();
        }
    }

    // Never sources delta_seconds internally - see physics_backend.hpp's own
    // doc comment on why a PhysicsBackend must never read a clock.
    void step([[maybe_unused]] float delta_seconds) noexcept {}

    [[nodiscard]] std::optional<BodyState> body_state(BodyId body) const noexcept {
        if (body.index >= bodies.size()) {
            return std::nullopt;
        }
        return bodies[body.index];
    }
};

static_assert(PhysicsBackend<NullPhysicsBackend>);

} // namespace atlas::physics
