#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace atlas {

// Stable resource identity value type (spec §3 Resource, §13
// atlas-resource: "resource identity, resource resolution, resource
// management" — this round covers identity only, see the library README
// for why resolution/management are deferred). A resource is looked up
// by this id rather than a hard-coded path: the id is a deterministic,
// pure function of the resource's authored stable name (FNV-1a over its
// bytes), so any two hosts computing it from the same name always agree,
// with no runtime discovery step involved (spec §4: determinism).
//
// Referenced directly as `atlas::ResourceId`, in the top-level `atlas`
// namespace rather than `atlas::resource`, the same as `atlas::EntityRef`
// — both are fundamental runtime vocabulary types expected to appear
// throughout generated contracts (e.g. a resource-typed property such as
// `ParticleEffects: ResourceList` holding a collection of these).
//
// A basic aggregate (rule of zero): a fixed-width, trivially copyable
// hash has no invariant to protect beyond the "null" sentinel (value 0,
// reserved for "no identity" and never produced by from_name for a
// non-empty name), so there's no reason to hide the field behind a class.
struct ResourceId {
    std::uint64_t value = 0;

    // Deterministically derives a ResourceId from a stable resource name
    // (e.g. "characters/hero/mesh") — never a filesystem path. Returns
    // the null id for an empty name rather than hashing zero bytes, so
    // "no identity" stays unambiguous rather than colliding with some
    // real name's hash.
    [[nodiscard]] static ResourceId from_name(std::string_view name) noexcept;

    [[nodiscard]] constexpr bool is_null() const noexcept { return value == 0; }

    friend constexpr auto operator<=>(const ResourceId&, const ResourceId&) = default;
};

} // namespace atlas

template <> struct std::hash<atlas::ResourceId> {
    // The id is already a well-distributed hash, so reuse it directly
    // rather than re-hashing an already-random value.
    std::size_t operator()(const atlas::ResourceId& id) const noexcept {
        return static_cast<std::size_t>(id.value);
    }
};
