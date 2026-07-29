#pragma once

#include "atlas/stage/stage_id.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace atlas::stage {

// Ordered, immutable sequence of StageId, walked in the exact same order on
// every traversal. This is the concrete mechanism behind the "fixed,
// deterministic sequence" §4 (Deterministic Execution) and §5 (Ordering
// Without Stages) both refer to: the runtime builds one of these once, then
// iterates it identically every tick — determinism requires no duplicate
// stage (an ambiguous "run twice" or "run out of order" case) and a
// reproducible iteration order (never re-derived from something unordered,
// e.g. a hash-based container).
//
// An encapsulated class rather than a plain aggregate: uniqueness of stages is
// a real invariant this type protects across its own construction, the same
// way atlas::entity::EntityRegistry protects slot/free-list consistency.
class StageSequence {
public:
    using const_iterator = std::vector<StageId>::const_iterator;

    // Returns std::nullopt if `stages` contains the same StageId more than
    // once, rather than silently de-duplicating or picking a winner.
    [[nodiscard]] static std::optional<StageSequence> create(std::vector<StageId> stages);

    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool contains(const StageId& stage) const noexcept;

private:
    explicit StageSequence(std::vector<StageId> stages) noexcept;

    std::vector<StageId> stages_;
};

} // namespace atlas::stage
