#include "atlas/entity/entity_registry.hpp"

namespace atlas::entity {

EntityRef EntityRegistry::create() {
    if (!free_indices_.empty()) {
        const auto index = free_indices_.back();
        free_indices_.pop_back();
        auto& slot = slots_[index];
        slot.alive = true;
        const auto ref = EntityRef{index, slot.generation};
        created_events_.push_back(ref);
        return ref;
    }

    const auto index = static_cast<EntityRef::IndexType>(slots_.size());
    slots_.push_back(Slot{.generation = 0, .alive = true});
    const auto ref = EntityRef{index, 0};
    created_events_.push_back(ref);
    return ref;
}

bool EntityRegistry::destroy(EntityRef ref) noexcept {
    if (!is_alive(ref)) {
        return false;
    }

    auto& slot = slots_[ref.index];
    slot.alive = false;
    ++slot.generation;
    free_indices_.push_back(ref.index);
    destroyed_events_.push_back(ref);
    return true;
}

bool EntityRegistry::is_alive(EntityRef ref) const noexcept {
    if (ref.is_null() || ref.index >= slots_.size()) {
        return false;
    }

    const auto& slot = slots_[ref.index];
    return slot.alive && slot.generation == ref.generation;
}

} // namespace atlas::entity
