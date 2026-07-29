#pragma once

#include "atlas/reflection/field_count.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace atlas::reflection {

// Cap on how many direct data members for_each_field()/field_types_t can
// walk. This is a different limit from detail::max_searched_fields in
// field_count.hpp (that one bounds a compile-time *search loop* and can be
// generous): the cap here bounds a hand-written dispatch table, because a
// structured binding's identifier list (`auto& [f0, ..., fN-1] = obj;`)
// cannot itself be produced by a template loop or a pack expansion — each
// supported count below is a distinct, textually-written case (see
// detail::tie_fields). Every hand-written contract struct in this repository
// today (atlas::EntityRef, atlas::contracts::ContractVersion, §21's
// Health/ApplyDamage/HealthChanged) has one or two fields; 16 leaves an 8x
// margin over that before a shape is simply unsupported, at the cost of 16
// cases written out by hand rather than an unbounded number.
inline constexpr std::size_t max_supported_fields = 16;

// Reflectable, plus a bound on field_count() so a type past
// max_supported_fields is rejected at the constraint (a substitution failure
// naming this concept), not by falling through detail::tie_fields's dispatch
// table into a static_assert deep inside a function body. The difference
// matters for testability: a constraint failure is a SFINAE-friendly
// substitution failure that a `requires` expression can probe without
// aborting the whole translation unit, which a hard static_assert inside an
// instantiated function body cannot (see field_visitor_test.cpp's cap-boundary
// test, which relies on exactly this).
template <typename T>
concept FieldVisitable =
    Reflectable<std::remove_cv_t<T>> && field_count<std::remove_cv_t<T>>() <= max_supported_fields;

namespace detail {

// Builds a std::tie(...)-style tuple aliasing T's direct data members, in
// declaration order. std::tie, never std::make_tuple: it binds references
// instead of copying/moving, so this works even for a move-only aggregate
// (e.g. field_count_test.cpp's MoveOnlyAggregate) where copying a field out
// would fail to compile. The if-constexpr chain is the "dispatch table keyed
// on the count" max_supported_fields' comment above refers to — one
// hand-written structured-binding case per supported count, since the
// binding's name list can't be generated any other way.
//
// FieldVisitable already keeps count within max_supported_fields for every
// caller reachable from outside this namespace; the trailing static_assert
// is defense-in-depth against a future internal caller that bypasses that
// constraint, not a path this library's own API can reach.
template <typename T> constexpr auto tie_fields([[maybe_unused]] T& obj) {
    constexpr std::size_t count = field_count<std::remove_cv_t<T>>();
    if constexpr (count == 0) {
        return std::tuple<>{};
    } else if constexpr (count == 1) {
        auto& [f0] = obj;
        return std::tie(f0);
    } else if constexpr (count == 2) {
        auto& [f0, f1] = obj;
        return std::tie(f0, f1);
    } else if constexpr (count == 3) {
        auto& [f0, f1, f2] = obj;
        return std::tie(f0, f1, f2);
    } else if constexpr (count == 4) {
        auto& [f0, f1, f2, f3] = obj;
        return std::tie(f0, f1, f2, f3);
    } else if constexpr (count == 5) {
        auto& [f0, f1, f2, f3, f4] = obj;
        return std::tie(f0, f1, f2, f3, f4);
    } else if constexpr (count == 6) {
        auto& [f0, f1, f2, f3, f4, f5] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5);
    } else if constexpr (count == 7) {
        auto& [f0, f1, f2, f3, f4, f5, f6] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6);
    } else if constexpr (count == 8) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7);
    } else if constexpr (count == 9) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8);
    } else if constexpr (count == 10) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9);
    } else if constexpr (count == 11) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
    } else if constexpr (count == 12) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11);
    } else if constexpr (count == 13) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12);
    } else if constexpr (count == 14) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
    } else if constexpr (count == 15) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14);
    } else if constexpr (count == 16) {
        auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15] = obj;
        return std::tie(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15);
    } else {
        static_assert(count <= max_supported_fields,
                      "atlas::reflection: T has more direct data members than "
                      "max_supported_fields supports; FieldVisitable should have "
                      "already rejected this at the call site");
        return std::tuple<>{};
    }
}

// Maps a std::tuple<Fields...> of (possibly const/reference-qualified) field
// types to std::tuple<std::decay_t<Fields>...> — the value-type shape
// field_types_t below exposes, since a caller inspecting "what type is this
// field" almost always wants the value type, not tie_fields' internal
// reference-tuple representation.
template <typename Tuple> struct decay_tuple_elements;

template <typename... Fields> struct decay_tuple_elements<std::tuple<Fields...>> {
    using type = std::tuple<std::decay_t<Fields>...>;
};

} // namespace detail

// Calls visitor(field) once per direct data member of obj, in declaration
// order — the structured-binding-based counterpart to field_count(): where
// field_count() only answers "how many", this walks the fields themselves,
// which is what §18 (Editor Extensions, "generic editing through generated
// reflection metadata") and §20 (Tooling Support, "display a property's full
// derivation without any property-specific tooling code") actually need.
//
// A generic (`auto&`) visitor gets each field's real type back through
// `decltype` on its own parameter — this is the field-*type*-inspection
// primitive this library can honestly build in C++23 (see this library's
// README, "Scoping decision"): not field *names* (that's C++26 P2996), just
// each field's real type, in order. field_types_t below packages the same
// information as a tuple type, for callers that want it without writing a
// visitor.
//
// obj is taken by T& so both a mutable object (visitor may write through
// its `auto&` parameter) and a const object (read-only display) work from
// the same function template; FieldVisitable constrains T (cv-qualification
// aside) to a plain aggregate within the supported field-count cap.
// visitor is deliberately never std::forward'd: it is invoked once per
// field, and forwarding is a single-use operation — forwarding it into any
// call but the last would risk leaving a moved-from callable behind for the
// rest. NOLINT below rather than restructuring around a forwarding call that
// would be semantically wrong here.
template <FieldVisitable T, typename Visitor>
constexpr void for_each_field(T& obj, Visitor&& visitor) { // NOLINT(cppcoreguidelines-missing-std-forward)
    std::apply([&visitor](auto&... fields) { (visitor(fields), ...); }, detail::tie_fields(obj));
}

// The reference-qualified tuple type detail::tie_fields would return for T —
// exposed as its own alias (rather than kept private) since field_types_t
// below is defined directly in terms of it, and a caller that genuinely wants
// aliasing references (not decayed value types) has a legitimate use for it
// too (e.g. building a std::tie-style view without a visitor).
template <FieldVisitable T> using field_reference_tuple_t = decltype(detail::tie_fields(std::declval<T&>()));

// std::tuple<...> of T's direct field types, in declaration order, decayed to
// value types (Field, not Field& or const Field&) — the type-level answer to
// the same question for_each_field's `decltype(field)` answers per call.
// Built from field_reference_tuple_t purely at the type level (decltype in an
// unevaluated context — std::declval is designed for exactly this): no T
// instance is ever constructed, so this works for move-only and
// non-default-constructible-beyond-Reflectable field types alike.
template <FieldVisitable T>
using field_types_t = typename detail::decay_tuple_elements<field_reference_tuple_t<T>>::type;

} // namespace atlas::reflection
