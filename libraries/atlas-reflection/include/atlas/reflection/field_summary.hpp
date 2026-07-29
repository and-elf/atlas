#pragma once

#include <cstddef>
#include <string>

namespace atlas::reflection {

// Renders a field_count() result for tooling/log display — e.g. "2 fields",
// "1 field", "0 fields". §20 (Tooling Support) is explicit that generic
// tooling should be able to display a reflected structure "without any
// property-specific tooling code"; this is that generic display's smallest
// building block, independent of which contract struct the count came from.
[[nodiscard]] std::string describe_field_count(std::size_t count);

} // namespace atlas::reflection
