#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

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
// "a source, a value, a priority, metadata, and a lifetime"). Started out
// keeping only what Additive/Multiplicative composition needed: a source
// (which capability contributed this, for future debugging/removal-by-name
// - nothing yet looks a contribution up by source), a value, and a lifetime
// tag. `priority` and `weight` were added once Priority Override and
// Weighted Composition (below) needed them - both default (0 and 1.0
// respectively) so every existing {.source = ..., .value = ...} call site
// remains unaffected: a contribution that never names a priority resolves
// as if it were the lowest-priority candidate, and one that never names a
// weight blends as if fully weighted on its own.
template <typename T> struct Contribution {
    std::string_view source;
    T value;
    Lifetime lifetime = Lifetime::Permanent;
    std::int32_t priority = 0;
    double weight = 1.0;
};

// Resolves an Additive-composed property's effective value: base plus every
// active contribution, summed in order (spec §20: "Armor: 100 + 50 (plate)
// + 20 (buff) = 170"). One of §20's seven composition strategies - see
// resolve_multiplicative, resolve_override, resolve_priority_override,
// resolve_set_union, resolve_ordered_composition, and
// resolve_weighted_composition below for the other six, each with
// genuinely different resolution semantics (a winner among candidates, an
// ordered merge, a proportional blend) that don't generalize from this one.
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

// Resolves an Override-composed property's effective value: one source
// replaces another outright (spec §20: "CurrentAnimation: Idle -> Attack
// (combat state overrides)"). With no active contributions the base wins by
// default; otherwise the most-recently-contributed value wins - the caller
// assembles `contributions` in contribution order (the same convention
// resolve_additive/resolve_multiplicative already rely on for their own
// ordering), so the last element in the span is the most recent one.
template <typename T>
[[nodiscard]] constexpr T resolve_override(T base, std::span<const Contribution<T>> contributions) {
    if (contributions.empty()) {
        return base;
    }
    return contributions.back().value;
}

// Resolves a Priority-Override-composed property's effective value: the
// highest-priority candidate wins among several (spec §20: "AnimationState:
// Stunned > Weapon > Default"). With no active contributions the base wins
// by default, standing in for the "Default" tier of that example. Ties
// break in favor of the later contribution in span order - consistent with
// resolve_override's own "most recent wins" tie-break, and with treating
// span order as contribution order throughout this header.
//
// This is the strategy spec §20's "Continuous Re-resolution and Preemption"
// section is written about: a higher-priority contribution preempts a
// lower-priority one immediately, without the loser needing to be removed
// first, simply because resolving again over the current active set picks
// a new winner. A preempted contribution is not deleted by this function -
// it was never touched - so it wins again immediately once the
// higher-priority contribution that preempted it is no longer part of the
// span passed in (e.g. because it expired or was withdrawn).
template <typename T>
[[nodiscard]] constexpr T resolve_priority_override(T base, std::span<const Contribution<T>> contributions) {
    if (contributions.empty()) {
        return base;
    }
    std::size_t winner_index = 0;
    for (std::size_t index = 1; index < contributions.size(); ++index) {
        if (contributions[index].priority >= contributions[winner_index].priority) {
            winner_index = index;
        }
    }
    return contributions[winner_index].value;
}

// Resolves a Set-Union-composed property's effective value: collections
// merge (spec §20: "Tags: [HeavyArmor] ∪ [Blessed] = [HeavyArmor,
// Blessed]"). `T` is itself a collection here (e.g. std::vector<Tag>) -
// unlike resolve_additive/resolve_multiplicative, where every contribution
// scales a single scalar `T`, each Set Union contribution supplies a whole
// collection of elements a single source wants present. The result starts
// as `base` and gains every contribution's elements in turn, skipping any
// element already present (by either an earlier contribution or the base
// itself) so the same tag contributed by two unrelated sources still
// appears exactly once - a duplicate contribution is not a distinct fact
// about the union.
template <typename T>
[[nodiscard]] constexpr T resolve_set_union(T base, std::span<const Contribution<T>> contributions) {
    T result = base;
    for (const auto& contribution : contributions) {
        for (const auto& element : contribution.value) {
            if (std::find(result.begin(), result.end(), element) == result.end()) {
                result.push_back(element);
            }
        }
    }
    return result;
}

// Resolves an Ordered-Composition-composed property's effective value:
// order of contribution matters (spec §20: "MaterialLayers: Skin -> Tattoo
// -> Armor -> DamageOverlay"). Unlike resolve_set_union, `T` here is a
// single element (one layer), not a collection - each contribution supplies
// one layer, and the effective value is the ordered sequence starting with
// the base layer and continuing with every contribution's layer in span
// order, duplicates and all: layering is about position, not uniqueness, so
// nothing here dedups the way resolve_set_union does.
template <typename T>
[[nodiscard]] constexpr std::vector<T>
resolve_ordered_composition(T base, std::span<const Contribution<T>> contributions) {
    std::vector<T> ordered;
    ordered.reserve(contributions.size() + 1);
    ordered.push_back(base);
    for (const auto& contribution : contributions) {
        ordered.push_back(contribution.value);
    }
    return ordered;
}

// Resolves a Weighted-Composition-composed property's effective value:
// contributions blend proportionally (spec §20: "AnimationPose: 70% Walk,
// 30% Run"). With no active contributions the base wins by default; with at
// least one contribution the result is `base`'s own value replaced by the
// weighted average of every contribution's value, normalized by the total
// weight rather than requiring weights to already sum to 1 - contributing
// 7.0/3.0 blends identically to contributing 0.7/0.3. A total weight of
// exactly 0 (e.g. every contributor's weight is 0, or none carries a
// non-zero weight) has no meaningful blend to compute, so the base is
// returned rather than dividing by zero.
template <typename T>
[[nodiscard]] constexpr T resolve_weighted_composition(T base,
                                                       std::span<const Contribution<T>> contributions) {
    if (contributions.empty()) {
        return base;
    }
    double weighted_sum = 0.0;
    double weight_total = 0.0;
    for (const auto& contribution : contributions) {
        weighted_sum += static_cast<double>(contribution.value) * contribution.weight;
        weight_total += contribution.weight;
    }
    if (weight_total == 0.0) {
        return base;
    }
    return static_cast<T>(weighted_sum / weight_total);
}

} // namespace atlas::runtime
