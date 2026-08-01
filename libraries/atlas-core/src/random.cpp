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

std::int64_t Random::next_in_range_i64(std::int64_t min, std::int64_t max) {
    if (min > max) {
        throw std::invalid_argument("Random::next_in_range_i64: min must be <= max");
    }

    // Reinterpreting each signed bound as its same-width unsigned bit
    // pattern turns "signed range [min, max]" into "unsigned span
    // umax - umin", computed via the well-defined (since C++20) two's-
    // complement modular arithmetic of unsigned subtraction/addition —
    // exactly the trick next_in_range() already uses to avoid representing
    // 2^64 directly, just one layer up. Delegating to next_in_range(0,
    // span) reuses its rejection-sampling bias elimination unchanged rather
    // than re-deriving it for a second numeric type.
    const auto umin = static_cast<std::uint64_t>(min);
    const auto umax = static_cast<std::uint64_t>(max);
    const std::uint64_t span = umax - umin;

    const std::uint64_t offset = next_in_range(0, span);
    return static_cast<std::int64_t>(umin + offset);
}

double Random::next_double() noexcept {
    // Classic 53-bit-mantissa technique: a double's mantissa holds 53 bits
    // of precision, so keep the high 53 bits of one next_u64() draw
    // (shifting off the low 11, which a 64-bit engine output has to spare)
    // and scale into [0, 1) by multiplying by 2^-53. 2^-53 is exactly
    // representable in IEEE-754 binary64, so that single multiplication is
    // the only rounding step involved, and it follows the same
    // round-to-nearest default on every conforming platform this project
    // targets (§4; `-ffp-contract=off`, set unconditionally per CLAUDE.md,
    // stops a fused multiply-add from folding this into a different
    // rounding step on FMA-capable hardware). std::uniform_real_distribution
    // is deliberately avoided for the same reason next_in_range()'s doc
    // comment gives for avoiding std::uniform_int_distribution: the standard
    // specifies it by output distribution only, not by a required
    // bit-exact algorithm.
    constexpr double scale = 0x1.0p-53;
    const std::uint64_t bits = next_u64() >> 11U;
    return static_cast<double>(bits) * scale;
}

double Random::next_double_in_range(double min, double max) {
    // NaN-safe: any comparison involving NaN is false, so `!(min <= max)`
    // (unlike `min > max`) also rejects a NaN bound instead of silently
    // propagating one into the result.
    if (!(min <= max)) {
        throw std::invalid_argument("Random::next_double_in_range: min must be <= max and not NaN");
    }

    // No degenerate-range special case needed: when min == max the span is
    // exactly 0.0, and next_double() * 0.0 is exactly 0.0 for any finite
    // next_double() result, so min + 0.0 already returns min exactly.
    return min + next_double() * (max - min);
}

} // namespace atlas::core
