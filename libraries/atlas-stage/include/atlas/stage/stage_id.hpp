#pragma once

#include <string_view>

namespace atlas::stage {

// Identifies a single runtime-internal execution stage (spec §13: "execution
// stages, lifecycle organization, deterministic ordering boundaries"). This is
// not a mechanism capabilities declare themselves into — §5 (Ordering Without
// Stages) is explicit that capability ordering comes entirely from the
// depends_on graph. StageId instead names one step of the fixed internal
// sequence the runtime itself walks every tick (§4, Deterministic Execution).
//
// A basic aggregate (rule of zero): identity is just the name, so there's no
// invariant here needing a constructor. `name` is expected to reference a
// string with static storage duration (a string literal), matching how stage
// names are defined by the runtime itself rather than assembled at runtime.
struct StageId {
    std::string_view name;

    friend constexpr bool operator==(const StageId&, const StageId&) noexcept = default;
};

} // namespace atlas::stage
