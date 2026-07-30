#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <string_view>

namespace atlas {

// Stable property identity value type - the wire/replication boundary's
// vocabulary for naming a property generically (issue #18, continuing the
// architecture discussion behind #16/#17): a server's replication layer
// deals in `(EntityId, PropertyId, value)` tuples and never needs to know
// what a given PropertyId *means* - that's entirely a client/content-layer
// concern, the same way `atlas::ResourceId` names a resource without the
// runtime understanding what it is. Deliberately the same shape as
// `atlas::ResourceId` (`atlas-resource/include/atlas/resource/resource_id.hpp`):
// a deterministic, pure function of an authored name (FNV-1a over its
// bytes), so any two hosts computing it from the same name always agree,
// with no runtime discovery step (spec §4: determinism).
//
// Physically owned by `atlas-replication` rather than a dedicated
// "atlas-property" library - unlike EntityRef (atlas-entity) or ResourceId
// (atlas-resource), a property has no other domain-identity library today;
// replication is PropertyId's sole reason to exist and its sole consumer,
// so it lives where it's used (issue #18) rather than presupposing a future
// library nothing yet demands.
//
// FNV-1a is duplicated here rather than shared with ResourceId::from_name's
// own copy (`atlas-resource/src/resource_id.cpp`) - both are small (~10
// line), independent pure functions with no shared state, and this is only
// the second vocabulary-identity type to need one. Extracting a shared
// helper (e.g. into atlas-core) is the right call once a third one shows up
// and duplication would mean three copies, not two - the same
// "a second real caller justifies it" reasoning this codebase already
// applies elsewhere (see demo/README.md's discussion of `haste`'s
// direct-assignment vs. a shared contribution registry).
//
// A basic aggregate (rule of zero): a fixed-width, trivially copyable hash
// has no invariant to protect beyond the "null" sentinel (value 0, reserved
// for "no identity" and never produced by from_name for a non-empty name).
struct PropertyId {
    std::uint64_t value = 0;

    // Deterministically derives a PropertyId from a stable property name
    // (e.g. "Health", "CastSpeed") - never a hard-coded numeric id. Returns
    // the null id for an empty name rather than hashing zero bytes, so
    // "no identity" stays unambiguous rather than colliding with some real
    // name's hash.
    [[nodiscard]] static PropertyId from_name(std::string_view name) noexcept;

    [[nodiscard]] constexpr bool is_null() const noexcept { return value == 0; }

    friend constexpr auto operator<=>(const PropertyId&, const PropertyId&) = default;
};

} // namespace atlas

template <> struct std::hash<atlas::PropertyId> {
    // The id is already a well-distributed hash, so reuse it directly
    // rather than re-hashing an already-random value (mirrors
    // std::hash<atlas::ResourceId>).
    std::size_t operator()(const atlas::PropertyId& id) const noexcept {
        return static_cast<std::size_t>(id.value);
    }
};
