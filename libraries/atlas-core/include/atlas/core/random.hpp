#pragma once

#include <cstdint>
#include <random>

namespace atlas::core {

// Deterministic, explicitly-seeded pseudo-random stream (spec §4,
// Determinism Constraints: "the per-host seeded random stream is the sole
// permitted randomness source"). This type is a single host's stream, not a
// global: owning and seeding it is a host/runtime concern (a future
// atlas-runtime responsibility), never ambient state this library provides
// itself.
//
// Engine choice: std::mt19937_64. Unlike most of the standard library's
// random-number facilities, std::mersenne_twister_engine's transition and
// generation algorithm is specified by the standard as exact formulas over
// its template parameters (word size, state size, tempering constants, ...),
// not left to "produces values with such-and-such statistical property."
// Two conforming standard libraries seeded with the same value are therefore
// required to produce bit-identical output *sequences* from the engine
// itself — exactly the cross-machine determinism spec §4 requires, and
// verifiable directly from the standard's wording rather than assumed.
//
// That guarantee does NOT extend to std::uniform_int_distribution or the
// other <random> distributions: the standard specifies those only by the
// mathematical distribution they must approximate, not by a required
// bit-exact algorithm for turning engine output into a bounded value. Two
// conforming standard libraries (e.g. libstdc++ on Linux vs. libc++ on macOS
// vs. MSVC STL on Windows — all three are real deployment targets per
// CLAUDE.md) are free to implement that mapping differently, and in practice
// do. Using std::uniform_int_distribution here would silently reintroduce
// the exact cross-platform non-determinism this type exists to eliminate.
// next_in_range() therefore implements its own bounding directly on top of
// next_u64() (see random.cpp) instead of reaching for a distribution.
//
// An encapsulated class, not a rule-of-zero aggregate: the invariant this
// protects is "the underlying engine only ever advances through this type's
// own draw methods," never re-seeded or inspected mid-stream by a caller —
// exposing the engine directly would let two capabilities desync a stream
// they're meant to share deterministically in a fixed order (spec §4).
class Random {
public:
    explicit Random(std::uint64_t seed) : engine_(seed) {}

    // Every next_*() call advances the underlying engine by exactly one
    // step, regardless of the width requested — a capability author
    // reasoning about "how many draws has this stream consumed" (spec §4's
    // fixed per-host consumption order) never has to account for a call
    // silently taking zero or two engine steps depending on which method it
    // was.
    [[nodiscard]] std::uint64_t next_u64() noexcept { return engine_(); }

    [[nodiscard]] std::uint32_t next_u32() noexcept { return static_cast<std::uint32_t>(engine_()); }

    // Returns a value uniformly distributed over [min, max] (inclusive).
    // Throws std::invalid_argument if min > max (CLAUDE.md: throw for
    // fallible operations rather than reach for std::expected, which this
    // toolchain's libstdc++/Clang combination cannot compile). May consume
    // more than one engine step per call when [min, max] doesn't evenly
    // divide the engine's output range (see random.cpp) — an inherent
    // property of unbiased rejection sampling, not a defect.
    [[nodiscard]] std::uint64_t next_in_range(std::uint64_t min, std::uint64_t max);

    // Signed-range counterpart of next_in_range() (§87: capability code
    // wants signed rolls, e.g. a damage delta expressed as `int`). A same-
    // named overload set on (std::uint64_t, std::uint64_t) vs. (std::int64_t,
    // std::int64_t) would make an ordinary call like `next_in_range(10, 20)`
    // ambiguous — both `int` arguments convert to either parameter type by
    // an equally-ranked integral conversion — so this is a distinctly named
    // method rather than an overload. Returns a value uniformly distributed
    // over [min, max] (inclusive); throws std::invalid_argument if
    // min > max. Built on next_in_range() itself (see random.cpp) rather
    // than a separate bounding implementation, so it inherits the same
    // unbiased-rejection-sampling guarantee without duplicating it.
    [[nodiscard]] std::int64_t next_in_range_i64(std::int64_t min, std::int64_t max);

    // Returns a double uniformly distributed over [0, 1). Deliberately not
    // std::uniform_real_distribution — see random.cpp for why that would
    // reintroduce the same cross-platform non-determinism next_in_range()'s
    // doc comment (and this library's README) already call out for
    // std::uniform_int_distribution.
    [[nodiscard]] double next_double() noexcept;

    // Floating-point counterpart of next_in_range(): returns a double
    // uniformly distributed over [min, max) — half-open, matching the
    // convention std::uniform_real_distribution itself uses, and the
    // natural range of the next_double() * span transform this is built on
    // (see random.cpp). Throws std::invalid_argument if min > max or either
    // bound is NaN.
    [[nodiscard]] double next_double_in_range(double min, double max);

private:
    std::mt19937_64 engine_;
};

} // namespace atlas::core
