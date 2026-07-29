#include "atlas/stage/stage_sequence.hpp"

#include <algorithm>

namespace atlas::stage {

StageSequence::StageSequence(std::vector<StageId> stages) noexcept : stages_(std::move(stages)) {}

std::optional<StageSequence> StageSequence::create(std::vector<StageId> stages) {
    // Duplicate detection sorts a *copy* by name; the public iteration order
    // exposed via begin()/end() always stays the caller's original insertion
    // order, never this sort order.
    auto sorted_by_name = stages;
    std::ranges::sort(sorted_by_name, {}, &StageId::name);
    if (std::ranges::adjacent_find(sorted_by_name, {}, &StageId::name) != sorted_by_name.end()) {
        return std::nullopt;
    }

    return StageSequence(std::move(stages));
}

StageSequence::const_iterator StageSequence::begin() const noexcept {
    return stages_.begin();
}

StageSequence::const_iterator StageSequence::end() const noexcept {
    return stages_.end();
}

std::size_t StageSequence::size() const noexcept {
    return stages_.size();
}

bool StageSequence::contains(const StageId& stage) const noexcept {
    return std::ranges::find(stages_, stage) != stages_.end();
}

} // namespace atlas::stage
