#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"

#include <optional>

namespace atlas::ui {

// The bindable-property mechanism §19 (Minimum UI Contract) requires: "any
// node property (visible, color, text, value) may be bound to a composed
// game property (§20), re-evaluated whenever that property's effective
// value changes (§20, Continuous Re-resolution)". This is deliberately not
// a parallel/stub property system - `resolve()` reads straight through
// atlas::Context::get<T>(), the same typed, monadic access path any
// non-UI capability's request handler already uses (§21), against the same
// atlas::runtime::PropertyStore<T> instances a real capability composition
// registers. A UI node binding to `MovementSpeed` and a movement capability
// reading `MovementSpeed` are reading the exact same composed value through
// the exact same storage - there is no second, UI-only notion of what a
// property is.
//
// "Re-evaluated whenever the effective value changes" is satisfied by never
// caching what `resolve()` last returned: every call re-reads the store, so
// there is nothing to invalidate when a contribution is added or removed
// elsewhere (spec §20, Continuous Re-resolution - resolution itself, not a
// push notification, is what "continuous" means here, the same way
// `resolve_additive`/`resolve_multiplicative` themselves have no cache).
//
// A basic aggregate (rule of zero): `value` (the literal/fallback) and
// `bound_entity` (nullopt when unbound) are two independent, always-valid
// fields - which one `resolve()` reads from is ordinary branching logic, not
// an invariant a constructor would need to protect. Same shape as
// atlas::EntityRef/atlas::ResourceId: public fields, a couple of small
// `[[nodiscard]]` helper methods, no private state.
template <typename T> struct BindableProperty {
    T value{};
    std::optional<atlas::EntityRef> bound_entity{};

    [[nodiscard]] bool is_bound() const noexcept { return bound_entity.has_value(); }

    // Resolves this property's effective value for the current tick. Falls
    // back to the literal `value` when unbound, or when bound but the
    // target entity has no stored value yet for T - both are ordinary,
    // expected outcomes (matching Context::get<T>()'s own nullopt
    // contract), not errors. Propagates Context::get<T>()'s
    // std::logic_error when no PropertyStore<T> was ever registered at all
    // - that is a composition/setup mistake, not a per-binding absence, and
    // deserves the same louder failure Context itself already gives it.
    [[nodiscard]] T resolve(atlas::Context& ctx) const {
        if (!bound_entity) {
            return value;
        }
        const auto found = ctx.get<T>(*bound_entity);
        if (!found) {
            return value;
        }
        return found->get();
    }
};

} // namespace atlas::ui
