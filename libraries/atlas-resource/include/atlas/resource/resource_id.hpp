#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace atlas {

// FNV-1a 64-bit - a deterministic, pure function of its input bytes with no
// platform entropy or iteration-order dependence, satisfying spec §4's
// determinism constraints for any value replicated or compared across
// hosts. constexpr (issue #23) so ResourceId::from_name can be evaluated at
// compile time, e.g. in a static_assert or as a template non-type
// parameter - every operation here (iteration over a string_view, XOR,
// unsigned multiply, static_cast<unsigned char>) is already constexpr-legal
// in C++23.
//
// Named (not anonymous) despite being private to this header: an anonymous
// namespace in a header gives each including translation unit its own
// internal-linkage copy, which is the wrong shape for something now meant
// to be constant-evaluated from any including TU. Not shared with
// atlas::PropertyId's own identical-looking copy
// (atlas-replication/include/atlas/replication/property_id.hpp) - see that
// header's own comment for why duplicating a second ~10-line pure function
// beats extracting a shared helper after only two callers exist.
namespace resource_id_detail {

inline constexpr std::uint64_t fnv_offset_basis = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t fnv_prime = 0x100000001b3ULL;

constexpr std::uint64_t fnv1a64(std::string_view text) noexcept {
    std::uint64_t hash = fnv_offset_basis;
    for (const char raw_byte : text) {
        hash ^= static_cast<unsigned char>(raw_byte);
        hash *= fnv_prime;
    }
    return hash;
}

} // namespace resource_id_detail

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
    [[nodiscard]] static constexpr ResourceId from_name(std::string_view name) noexcept {
        if (name.empty()) {
            return ResourceId{};
        }
        return ResourceId{resource_id_detail::fnv1a64(name)};
    }

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
