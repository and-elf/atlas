#pragma once

#include "atlas/entity/entity_ref.hpp"

#include <cstdint>
#include <span>
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

    // Lifecycle hooks (issue #105): entities created/destroyed since the
    // last clear_lifecycle_events() call, exposed as plain data for a caller
    // to poll during its own scheduled turn — e.g. a property-store cleanup
    // pass reading destroyed_since_last_poll() to drop stale per-entity
    // entries. Deliberately not a callback/subscriber list — see this
    // library's README ("Lifecycle Hooks") for why a poll-based shape
    // matches this codebase's established "no separate delivery mechanism"
    // direction (spec §20, Triggered Composition) better than inventing one.
    //
    // Both lists accumulate in call order and keep growing until
    // clear_lifecycle_events() is called — there is no implicit tick
    // boundary inside EntityRegistry itself, so a caller that never polls
    // (or never clears) simply accumulates the full history.
    [[nodiscard]] std::span<const EntityRef> created_since_last_poll() const noexcept {
        return created_events_;
    }

    [[nodiscard]] std::span<const EntityRef> destroyed_since_last_poll() const noexcept {
        return destroyed_events_;
    }

    // Empties both lists. Called by whoever just consumed them — the same
    // "read once, then discard" shape a triggered property's tick-boundary
    // reset already uses (spec §20, Triggered Composition), just driven by
    // the caller's own polling cadence rather than a Context/tick concept
    // this library has no dependency on.
    void clear_lifecycle_events() noexcept {
        created_events_.clear();
        destroyed_events_.clear();
    }

private:
    struct Slot {
        EntityRef::GenerationType generation = 0;
        bool alive = false;
    };

    std::vector<Slot> slots_;
    std::vector<EntityRef::IndexType> free_indices_;
    std::vector<EntityRef> created_events_;
    std::vector<EntityRef> destroyed_events_;
};

} // namespace atlas::entity
