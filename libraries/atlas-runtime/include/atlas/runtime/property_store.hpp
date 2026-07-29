#pragma once

#include "atlas/entity/entity_ref.hpp"

#include <functional>
#include <optional>
#include <unordered_map>

namespace atlas::runtime {

// Generic per-entity storage for one property type T - the storage side of
// atlas::Context::get<T>() (see context.hpp). Domain-agnostic: PropertyStore
// itself never knows whether T is Health, Armor, or anything else (spec §2,
// Mechanism Over Meaning) - it only needs T to be storable, which every
// PropertyContract already is (semiregular, atlas-contracts).
//
// Rule of Zero doesn't apply here even though this has no invariant beyond
// what std::unordered_map itself protects: it's kept as a thin class (not a
// bare unordered_map alias) so get()'s std::optional<reference_wrapper>
// return shape - the "does this entity have this property" question a
// caller actually asks - is part of the type's own interface rather than
// something every call site re-derives from an iterator comparison.
template <typename T> class PropertyStore {
public:
    // Creates (or overwrites) this entity's value. Returns a reference to
    // the stored value in case the caller wants to continue working with it
    // (e.g. immediately handing it to a composition engine).
    T& set(EntityRef entity, T value) {
        auto [it, inserted] = values_.insert_or_assign(entity, std::move(value));
        return it->second;
    }

    [[nodiscard]] std::optional<std::reference_wrapper<T>> get(EntityRef entity) noexcept {
        const auto it = values_.find(entity);
        if (it == values_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::optional<std::reference_wrapper<const T>> get(EntityRef entity) const noexcept {
        const auto it = values_.find(entity);
        if (it == values_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    std::unordered_map<EntityRef, T> values_;
};

} // namespace atlas::runtime
