#pragma once

#include <cstdint>

namespace atlas::physics {

// A stable, generation-checked handle to a body a PhysicsBackend owns -
// mirrors atlas::EntityRef's own index+generation handle pattern exactly
// (libraries/atlas-entity/include/atlas/entity/entity_ref.hpp): a plain
// index into a backend's own internal body storage, paired with a
// generation counter so a stale handle to a destroyed-and-slot-reused body
// is distinguishable from a handle to the body currently occupying that
// slot. A basic aggregate (rule of zero): no invariant here needs a
// constructor to protect beyond the default "null" value, so there's no
// reason to hide the fields behind one.
//
// Whether and how a given PhysicsBackend actually reuses a destroyed body's
// index (and therefore whether generation ever needs to advance past 0) is
// entirely that backend's own concern - NullPhysicsBackend never reuses an
// index at all (see its own doc comment), while a real backend (#178/#179's
// Jolt-backed one) is free to run its own free-list/generation-bump
// strategy the exact same way atlas::entity::EntityRegistry does for
// EntityRef. This contract only fixes the handle's *shape*, never a reuse
// policy.
struct BodyId {
    using IndexType = std::uint32_t;
    using GenerationType = std::uint32_t;

    static constexpr IndexType null_index = static_cast<IndexType>(-1);

    IndexType index = null_index;
    GenerationType generation = 0;

    // The default-constructed BodyId never refers to a live body in any
    // PhysicsBackend - it is the "no body" sentinel.
    [[nodiscard]] constexpr bool is_null() const noexcept { return index == null_index; }

    friend constexpr bool operator==(const BodyId&, const BodyId&) noexcept = default;
};

} // namespace atlas::physics
