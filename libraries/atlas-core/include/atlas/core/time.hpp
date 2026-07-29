#pragma once

#include <chrono>
#include <compare>
#include <cstdint>

namespace atlas::core {

// Deterministic simulation-time value type (spec §4, Determinism Constraints:
// "Never read platform wall-clock time ... inside simulation logic. Use
// Atlas's built-in deterministic Time and Random types only"). A Time is a
// count of discrete simulation ticks, advanced only by the runtime's
// scheduler stepping the simulation forward — never by sampling
// std::chrono::system_clock/steady_clock, which would make outcomes depend
// on wall-clock timing and break bit-exact replay across machines.
//
// ticks_per_second is a fixed constant rather than a per-instance field: the
// rate is a property of a host's simulation configuration (a single, shared
// constant every capability composed into that host agrees on), not
// something an individual Time value carries. 60 is chosen as this library's
// default because it is a widely-used fixed-timestep baseline for real-time
// simulations, but no consumer should treat it as spec-mandated — see the
// README's "Open questions" for whether this should become a host-level
// composition parameter instead of a library-wide constant.
//
// A basic aggregate (rule of zero): the only invariant here is "ticks never
// represents wall-clock time," which is a usage discipline for callers, not
// something a constructor can enforce — so this stays public fields, no
// hand-rolled special member functions.
struct Time {
    std::uint64_t ticks = 0;

    static constexpr std::uint64_t ticks_per_second = 60;

    friend constexpr auto operator<=>(const Time&, const Time&) = default;

    [[nodiscard]] constexpr Time operator+(std::uint64_t delta_ticks) const noexcept {
        return Time{ticks + delta_ticks};
    }

    constexpr Time& operator+=(std::uint64_t delta_ticks) noexcept {
        ticks += delta_ticks;
        return *this;
    }

    // Signed so that subtracting a later Time from an earlier one (a caller
    // ordering mistake, not a wraparound this type should silently absorb)
    // reads as a negative delta instead of wrapping to a huge unsigned value.
    [[nodiscard]] constexpr std::int64_t operator-(const Time& other) const noexcept {
        return static_cast<std::int64_t>(ticks) - static_cast<std::int64_t>(other.ticks);
    }

    // Presentation-only conversion (spec §4: wall-clock-shaped time is fine
    // for audio/render interpolation as long as it never feeds back into
    // simulation state) — never use the returned duration to drive
    // simulation logic.
    [[nodiscard]] constexpr std::chrono::duration<double> to_duration() const noexcept {
        return std::chrono::duration<double>(static_cast<double>(ticks) /
                                             static_cast<double>(ticks_per_second));
    }
};

} // namespace atlas::core
