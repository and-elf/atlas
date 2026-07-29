#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace atlas {

// Fundamental runtime vocabulary type, referenced directly as `atlas::EntityRef`
// by generated request/event contracts (spec §21) — hence the top-level `atlas`
// namespace rather than `atlas::entity`, even though this header is owned by
// and physically lives under the atlas-entity library.
//
// A basic aggregate (rule of zero): no invariant here needs a constructor to
// protect beyond the default "null" value, so there's no reason to hide the
// fields behind one. EntityRegistry (which does protect a real invariant —
// slot/free-list consistency) stays a proper encapsulated class.
struct EntityRef {
    using IndexType = std::uint32_t;
    using GenerationType = std::uint32_t;

    static constexpr IndexType null_index = static_cast<IndexType>(-1);

    IndexType index = null_index;
    GenerationType generation = 0;

    // The default-constructed EntityRef never refers to a live entity in any
    // EntityRegistry — it is the "no entity" sentinel (e.g. an unset request target).
    [[nodiscard]] constexpr bool is_null() const noexcept { return index == null_index; }

    friend constexpr bool operator==(const EntityRef&, const EntityRef&) noexcept = default;
};

} // namespace atlas

template <> struct std::hash<atlas::EntityRef> {
    // Combines both fields (unlike atlas::ResourceId's hash, which reuses
    // its single already-random field directly) - index and generation are
    // each ordinary small counters, neither well-distributed alone, so both
    // must contribute or two live entities sharing an index at different
    // generations (or vice versa) would collide far more than a hash
    // function should. XOR-with-a-shifted-generation is the same
    // combination technique boost::hash_combine popularized, sized for
    // exactly two 32-bit fields rather than pulled in as a dependency for
    // one call site.
    std::size_t operator()(const atlas::EntityRef& ref) const noexcept {
        return static_cast<std::size_t>(ref.index) ^ (static_cast<std::size_t>(ref.generation) << 1U);
    }
};
