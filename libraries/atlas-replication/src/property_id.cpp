#include "atlas/replication/property_id.hpp"

namespace atlas {

namespace {

// FNV-1a 64-bit - see property_id.hpp's header comment for why this isn't
// shared with atlas::ResourceId::from_name's own copy yet.
constexpr std::uint64_t fnv_offset_basis = 0xcbf29ce484222325ULL;
constexpr std::uint64_t fnv_prime = 0x100000001b3ULL;

std::uint64_t fnv1a64(std::string_view text) noexcept {
    std::uint64_t hash = fnv_offset_basis;
    for (const char raw_byte : text) {
        hash ^= static_cast<unsigned char>(raw_byte);
        hash *= fnv_prime;
    }
    return hash;
}

} // namespace

PropertyId PropertyId::from_name(std::string_view name) noexcept {
    if (name.empty()) {
        return PropertyId{};
    }
    return PropertyId{fnv1a64(name)};
}

} // namespace atlas
