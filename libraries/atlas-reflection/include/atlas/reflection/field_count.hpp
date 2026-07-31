#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace atlas::reflection {

// A type this library can count the direct data members of: a plain
// aggregate (the Rule-of-Zero shape every hand-written contract struct in
// this repo already follows — atlas::EntityRef, atlas::contracts::
// ContractVersion, and the properties/requests/events §21's worked example
// generates) that is also default-constructible, since field_count() below
// relies on `T{}` itself being well-formed as its search's starting point.
//
// Deliberately not atlas::ContractStruct (atlas-contracts): per §5, contracts
// sit *above* runtime libraries in the dependency chain (Capability ->
// Contracts -> Runtime -> Platform), so a runtime library such as this one
// depending on atlas-contracts would invert that direction. Reflectable is
// intentionally close to ContractStruct's definition without literally being
// it — see this library's README for the reasoning.
//
// Note what this concept does *not* rule out: is_aggregate_v<T> permits
// public base classes, C-style array members, bit-fields, and reference
// members just as readily as a flat struct of scalars. field_count() remains
// callable — and gives a well-defined answer — for all of those; it is just
// not always the answer a human would expect. See field_count()'s own
// comment and this library's README ("Known Limitations") for exactly which
// shapes count correctly and which don't.
template <typename T>
concept Reflectable = std::is_aggregate_v<T> && std::default_initializable<T>;

namespace detail {

// Converts to (almost) any type — used only inside an unevaluated
// requires-expression (brace_constructible_with, below) to probe how many
// elements an aggregate's brace-init accepts. Never defined: it only needs to
// be a valid conversion candidate for overload resolution to consider, never
// actually invoked, since a requires-expression checks well-formedness
// without evaluating anything inside it.
struct AnyMember {
    template <typename member_type>
    constexpr operator member_type() const noexcept; // NOLINT(*) — intentionally undefined
};

// Binds one placeholder per index in a parameter pack expansion below,
// ignoring the index itself — the only way to get N repetitions of the same
// type out of a pack expansion.
template <std::size_t> using any_member_at = AnyMember;

template <typename T, std::size_t... indices>
constexpr bool brace_constructible_with(std::index_sequence<indices...> /*unused*/) {
    return requires { T{any_member_at<indices>{}...}; };
}

// Upper bound the search below climbs to. Every hand-written contract struct
// in this repository today (atlas::EntityRef, atlas::contracts::
// ContractVersion, and §21's Health/ApplyDamage/HealthChanged) has one or two
// fields; this leaves generous headroom for real capability contracts while
// keeping a runaway type from recursing indefinitely at compile time.
//
// Kept twice field_visitor.hpp's max_supported_fields (see that file's
// comment for why the two limits differ) so a type one field past the
// dispatch-table cap is still counted *exactly* rather than clipped to the
// same value — the margin is what lets FieldVisitable reject it correctly
// instead of silently under-counting it down to the cap (see
// field_visitor_test.cpp's ThirtyThreeFields boundary test, which relies on
// field_count() reporting 33, not 32, one field past the cap).
inline constexpr std::size_t max_searched_fields = 64;

template <typename T, std::size_t count = 0> constexpr std::size_t field_count_from() {
    // Both the search-cap guard and "one more field doesn't fit" land on the
    // same "stop here" outcome, so they're combined into a single condition
    // rather than two branches with identical bodies.
    if constexpr (count < max_searched_fields &&
                  brace_constructible_with<T>(std::make_index_sequence<count + 1>{})) {
        return field_count_from<T, count + 1>();
    } else {
        return count;
    }
}

} // namespace detail

// Number of direct data members `T` accepts through aggregate initialization
// — the well-known "how many braces does it take" technique: probe
// increasing counts of an any-convertible placeholder (detail::AnyMember)
// until one more element stops the brace-init from compiling. This is the
// primitive §18 (Editor Extensions) and §20 (Property and Resource
// Composition, Tooling Support) need to walk a contract struct's shape
// generically — "tooling can display a property's full derivation without
// any property-specific tooling code" requires discovering that shape
// without per-type hand-written reflection.
//
// Known limits (verified empirically, pinned by tests, documented in this
// library's README rather than guarded against — no contract struct in this
// repository triggers any of them today):
//   - A base class subobject converts as a single opaque unit (AnyMember has
//     a valid conversion path directly to a class type), so it counts as
//     exactly one field regardless of how many members the base itself has —
//     neither "0, excluded" nor "flattened to the base's own count".
//   - A C-style array member has no such single-object conversion (a
//     conversion function cannot return an array type), so the compiler
//     brace-elides through it and the search instead counts the array's
//     flattened element count.
//   - Bit-field members count correctly (this technique never takes a
//     bit-field's address or binds a reference to it, unlike
//     structured-binding-based reflection techniques, where that would be a
//     hard failure).
//   - A reference member with no default member initializer already fails
//     the Reflectable constraint above (not default-constructible), so it is
//     never reachable here; one *with* a default member initializer is not
//     specially detected and is out of scope.
template <Reflectable T> consteval std::size_t field_count() {
    return detail::field_count_from<T>();
}

} // namespace atlas::reflection
