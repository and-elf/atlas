#include "atlas/core/random.hpp"

#include <limits>
#include <stdexcept>

namespace atlas::core {

std::uint64_t Random::next_in_range(std::uint64_t min, std::uint64_t max) {
    if (min > max) {
        throw std::invalid_argument("Random::next_in_range: min must be <= max");
    }

    const std::uint64_t span = max - min;
    if (span == std::numeric_limits<std::uint64_t>::max()) {
        return next_u64();
    }

    const std::uint64_t bound = span + 1;

    // Rejection-sampling bound derivation (the classic arc4random_uniform
    // approach): reject_below == 2^64 mod bound, computed via unsigned
    // wraparound to avoid representing 2^64 directly. Discarding draws below
    // this threshold makes every remaining draw's `draw % bound` land with
    // exactly equal probability on each of the bound distinct outputs —
    // without it, values in [0, 2^64 mod bound) would come up slightly more
    // often than the rest (modulo bias).
    const std::uint64_t reject_below = (std::numeric_limits<std::uint64_t>::max() - bound + 1) % bound;

    std::uint64_t draw = next_u64();
    while (draw < reject_below) {
        draw = next_u64();
    }

    return min + (draw % bound);
}

} // namespace atlas::core
