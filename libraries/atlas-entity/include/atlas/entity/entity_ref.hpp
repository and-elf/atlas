#pragma once

#include <cstdint>

namespace atlas {

// Fundamental runtime vocabulary type, referenced directly as `atlas::EntityRef`
// by generated request/event contracts (spec §21) — hence the top-level `atlas`
// namespace rather than `atlas::entity`, even though this header is owned by
// and physically lives under the atlas-entity library.
class EntityRef {
public:
    using IndexType = std::uint32_t;
    using GenerationType = std::uint32_t;

    constexpr EntityRef() noexcept = default;

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - conventional index/generation order
    constexpr EntityRef(IndexType index, GenerationType generation) noexcept
        : index_{index}, generation_{generation} {}

    [[nodiscard]] constexpr IndexType index() const noexcept { return index_; }
    [[nodiscard]] constexpr GenerationType generation() const noexcept { return generation_; }

    // The default-constructed EntityRef never refers to a live entity in any
    // EntityRegistry — it is the "no entity" sentinel (e.g. an unset request target).
    [[nodiscard]] constexpr bool is_null() const noexcept { return index_ == null_index; }

    friend constexpr bool operator==(const EntityRef&, const EntityRef&) noexcept = default;

private:
    static constexpr IndexType null_index = static_cast<IndexType>(-1);

    IndexType index_ = null_index;
    GenerationType generation_ = 0;
};

} // namespace atlas
