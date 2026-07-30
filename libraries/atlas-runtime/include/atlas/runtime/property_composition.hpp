#pragma once

#include <span>
#include <string_view>

namespace atlas::runtime {

// An independent input to a composed property (spec §20, Contribution:
// "a source, a value, a priority, metadata, and a lifetime"). This round
// keeps only what Additive composition actually needs: a source (which
// capability contributed this, for future removal/debugging - nothing yet
// looks a contribution up by source, so it's informational this round) and
// a value. Priority (needed for Priority Override) and lifetime (Permanent/
// Duration/UntilEvent/WhileCondition) are real spec §20 concepts this round
// deliberately does not implement - see this library's README.
template <typename T> struct Contribution {
    std::string_view source;
    T value;
};

// Resolves an Additive-composed property's effective value: base plus every
// active contribution, summed in order (spec §20: "Armor: 100 + 50 (plate)
// + 20 (buff) = 170"). The only composition strategy this round implements
// - atlas::Composition names six others (Multiplicative, Override,
// PriorityOverride, SetUnion, OrderedComposition, WeightedComposition) that
// have no evaluator yet; each has different resolution semantics (a winner
// among candidates, an ordered merge, ...) that don't generalize from this
// one, so they're each their own future increment rather than guessed at
// here.
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
