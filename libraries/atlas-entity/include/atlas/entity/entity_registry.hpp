#pragma once

#include "atlas/entity/entity_ref.hpp"

#include <cstdint>
#include <vector>

namespace atlas::entity {

// Generational-index entity manager (spec §13: "entity identity, entity
// lifecycle, entity management mechanisms"). A recycled index gets a bumped
// generation so a stale EntityRef from before a destroy()/create() cycle is
// never mistaken for the entity that now occupies its slot.
class EntityRegistry {
public:
    [[nodiscard]] EntityRef create();

    // Returns false if ref does not refer to a currently-alive entity
    // (already destroyed, stale generation, or never allocated) rather than
    // throwing or asserting — callers decide whether that's an error.
    bool destroy(EntityRef ref) noexcept;

    [[nodiscard]] bool is_alive(EntityRef ref) const noexcept;

private:
    struct Slot {
        EntityRef::GenerationType generation = 0;
        bool alive = false;
    };

    std::vector<Slot> slots_;
    std::vector<EntityRef::IndexType> free_indices_;
};

} // namespace atlas::entity
