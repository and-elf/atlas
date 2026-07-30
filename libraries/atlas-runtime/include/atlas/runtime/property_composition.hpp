#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace atlas::runtime {

// A Contribution's lifetime (spec §20, Contribution: "a source, a value, a
// priority, metadata, and a lifetime"). Permanent is every contribution
// added by this codebase so far (armor::add_contribution,
// movement::add_speed_contribution never set this explicitly - it's the
// default). WhileCondition is the one kind this round adds real meaning to:
// a contribution whose own governing condition (e.g. an aura's per-tick
// range check against its source entity) decides when it stops applying,
// removed via remove_contributions_by_source below the instant that
// condition fails - this library never evaluates the condition itself,
// only provides the removal mechanism a capability's own per-tick logic
// needs once it has decided to drop one. Duration and UntilEvent are real
// spec §20 concepts this round still does not implement - each needs its
// own associated data (a tick countdown; an event type to listen for) a
// bare enum tag can't carry, and nothing consumes them yet.
enum class Lifetime : std::uint8_t { Permanent, Duration, UntilEvent, WhileCondition };

// An independent input to a composed property (spec §20, Contribution:
// "a source, a value, a priority, metadata, and a lifetime"). This round
// keeps what Additive/Multiplicative composition and removal-by-source
// actually need: a source (which capability contributed this - looked up by
// remove_contributions_by_source below), a value, and a lifetime tag
// (defaulted to Permanent, so every existing {.source = ..., .value = ...}
// call site is unaffected). Priority (needed for Priority Override) is a
// real spec §20 concept this round still deliberately does not implement -
// see this library's README.
template <typename T> struct Contribution {
    std::string_view source;
    T value;
    Lifetime lifetime = Lifetime::Permanent;
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

// Removes every contribution whose source exactly matches source_label from
// contributions, returning how many were removed - the reverse of what
// every add_contribution-style capability function already does (push_back,
// then re-resolve). Nothing has needed this until now, since every
// contribution added so far has been Permanent (never removed on its own);
// a WhileCondition-lifetime contribution (e.g. an aura whose target just
// left range, spec §20) needs exactly this on the way out. Deliberately
// does not re-resolve the owning property itself - that's the caller's job,
// the same way it already is after adding a contribution, since which
// resolve_* to call depends on the composition strategy and this function
// has no reason to know that.
template <typename T>
[[nodiscard]] std::size_t remove_contributions_by_source(std::vector<Contribution<T>>& contributions,
                                                         std::string_view source_label) {
    return std::erase_if(contributions, [source_label](const Contribution<T>& contribution) {
        return contribution.source == source_label;
    });
}

} // namespace atlas::runtime
