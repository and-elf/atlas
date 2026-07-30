#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace atlas::runtime {

// A Contribution's lifetime (spec §20, Contribution: "a source, a value, a
// priority, metadata, and a lifetime") - purely descriptive metadata this
// round, not something any function here branches on. Permanent is every
// contribution added by this codebase so far (armor::add_contribution,
// movement::add_speed_contribution never set this explicitly - it's the
// default): added once by a discrete request (e.g. equip), never removed
// except by another discrete request, so nothing needs to re-derive it on
// a schedule. WhileCondition is different in a way that rules out a stored,
// incrementally-patched entry entirely: nothing ever fires an event when a
// governing condition (e.g. an aura's range check against its source) stops
// holding - nothing "tells" you a target walked out of range, since that's
// a fact about the current tick's world state, not an occurrence. So a
// WhileCondition contribution is never persisted at all - it's constructed
// fresh each tick by whichever capability owns the condition, fed straight
// into resolve_additive/resolve_multiplicative below alongside whatever
// Permanent contributions are already stored, and discarded immediately
// after. Tagging it WhileCondition here is documentation of *why* it exists
// only for one resolution's span<> call, not a value any code inspects.
// Duration and UntilEvent are real spec §20 concepts this round still does
// not implement - each needs its own associated data (a tick countdown; an
// event type to listen for) a bare enum tag can't carry, and nothing
// consumes them yet.
enum class Lifetime : std::uint8_t { Permanent, Duration, UntilEvent, WhileCondition };

// An independent input to a composed property (spec §20, Contribution:
// "a source, a value, a priority, metadata, and a lifetime"). This round
// keeps what Additive/Multiplicative composition actually needs: a source
// (which capability contributed this, for future debugging/removal-by-name
// - nothing yet looks a contribution up by source), a value, and a lifetime
// tag (defaulted to Permanent, so every existing {.source = ..., .value =
// ...} call site is unaffected). Priority (needed for Priority Override) is
// a real spec §20 concept this round still deliberately does not implement
// - see this library's README.
template <typename T> struct Contribution {
    std::string_view source;
    T value;
    Lifetime lifetime = Lifetime::Permanent;
};

// Resolves an Additive-composed property's effective value: base plus every
// active contribution, summed in order (spec §20: "Armor: 100 + 50 (plate)
// + 20 (buff) = 170"). One of two composition strategies this round
// implements (Multiplicative is the other, resolve_multiplicative below) -
// atlas::Composition names five others (Override, PriorityOverride,
// SetUnion, OrderedComposition, WeightedComposition) that have no evaluator
// yet; each has different resolution semantics (a winner among candidates,
// an ordered merge, ...) that don't generalize from either of these, so
// they're each their own future increment rather than guessed at here.
template <typename T>
[[nodiscard]] constexpr T resolve_additive(T base, std::span<const Contribution<T>> contributions) {
    T total = base;
    for (const auto& contribution : contributions) {
        total += contribution.value;
    }
    return total;
}

// Resolves a Multiplicative-composed property's effective value: base times
// every active contribution, folded in order (spec §20: "MovementSpeed:
// 10 x 0.5 (slow) x 1.2 (haste) = 6"). Unlike resolve_additive, a base of 0
// is not this strategy's identity - callers must pass the property's actual
// declared base (e.g. a MovementSpeed of 10.0), never a hardcoded 1.0/0.0
// stand-in, or the result silently loses the base entirely. See
// demo/modules/movement's README note for why this composition strategy in
// particular needs its declared base tracked separately from whatever
// mutable effective value a PropertyStore holds.
template <typename T>
[[nodiscard]] constexpr T resolve_multiplicative(T base, std::span<const Contribution<T>> contributions) {
    T total = base;
    for (const auto& contribution : contributions) {
        total *= contribution.value;
    }
    return total;
}

} // namespace atlas::runtime
