#include "atlas/diagnostics/severity.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace atlas::diagnostics {

std::string to_string(Severity severity) {
    // Indexed directly by the underlying enum value rather than a switch:
    // Severity's declaration order (severity.hpp) is exactly this array's
    // order, so this stays a single source of truth to keep in sync
    // rather than two (the enum, and a switch's case list) that could
    // drift apart silently if a level were ever reordered or inserted.
    static constexpr std::array<std::string_view, 5> names{"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};

    const auto index = static_cast<std::size_t>(severity);
    if (index >= names.size()) {
        throw std::invalid_argument("atlas::diagnostics::to_string: unknown Severity value");
    }
    // .at(), not operator[]: index is not a compile-time constant, and
    // cppcoreguidelines-pro-bounds-constant-array-index requires bounds-
    // checked access for a runtime index even though the check just above
    // already makes an out-of-range access unreachable here.
    return std::string(names.at(index));
}

} // namespace atlas::diagnostics
