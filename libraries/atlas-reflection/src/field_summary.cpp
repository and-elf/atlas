#include "atlas/reflection/field_summary.hpp"

#include <format>

namespace atlas::reflection {

std::string describe_field_count(std::size_t count) {
    return std::format("{} field{}", count, count == 1 ? "" : "s");
}

} // namespace atlas::reflection
