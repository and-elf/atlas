#include "atlas/resource/resource_id.hpp"

namespace atlas {

namespace {

// FNV-1a 64-bit: a deterministic, pure function of its input bytes with
// no platform entropy or iteration-order dependence, satisfying spec §4's
// determinism constraints for any value replicated or compared across
// hosts.
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

ResourceId ResourceId::from_name(std::string_view name) noexcept {
    if (name.empty()) {
        return ResourceId{};
    }
    return ResourceId{fnv1a64(name)};
}

} // namespace atlas
