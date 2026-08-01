#include "atlas/runtime/property_composition.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace atlas::runtime {
namespace {

TEST(ResolveAdditive, BaseAloneWithNoContributionsIsUnchanged) {
    EXPECT_EQ(resolve_additive<std::int32_t>(0, {}), 0);
    EXPECT_EQ(resolve_additive<std::int32_t>(100, {}), 100);
}

TEST(ResolveAdditive, SumsBaseAndEveryContribution) {
    // Reproduces spec §20's own Additive example: "Armor: 100 + 50 (plate)
    // + 20 (buff) = 170".
    const std::array<Contribution<std::int32_t>, 2> contributions{{
        {.source = "plate", .value = 50},
        {.source = "buff", .value = 20},
    }};

    EXPECT_EQ(resolve_additive<std::int32_t>(100, contributions), 170);
}

TEST(ResolveAdditive, SingleContributionMatchesTheIntegrationScenario) {
    // The scenario this engine is being built for: Armor starts at base 0
    // with a single +5 contribution (e.g. an equipped plate), giving an
    // effective value of 5.
    const std::array<Contribution<std::int32_t>, 1> contributions{{
        {.source = "plate", .value = 5},
    }};

    EXPECT_EQ(resolve_additive<std::int32_t>(0, contributions), 5);
}

TEST(ResolveAdditive, NegativeContributionsSubtract) {
    const std::array<Contribution<std::int32_t>, 1> contributions{{
        {.source = "corrosion", .value = -30},
    }};

    EXPECT_EQ(resolve_additive<std::int32_t>(100, contributions), 70);
}

TEST(ResolveMultiplicative, BaseAloneWithNoContributionsIsUnchanged) {
    EXPECT_FLOAT_EQ(resolve_multiplicative<float>(1.0F, {}), 1.0F);
    EXPECT_FLOAT_EQ(resolve_multiplicative<float>(10.0F, {}), 10.0F);
}

TEST(ResolveMultiplicative, MultipliesBaseByEveryContributionInOrder) {
    // Reproduces spec §20's own Multiplicative example: "MovementSpeed:
    // 10 x 0.5 (slow) x 1.2 (haste) = 6".
    const std::array<Contribution<float>, 2> contributions{{
        {.source = "slow", .value = 0.5F},
        {.source = "haste", .value = 1.2F},
    }};

    EXPECT_FLOAT_EQ(resolve_multiplicative<float>(10.0F, contributions), 6.0F);
}

TEST(ResolveMultiplicative, SingleContributionScalesTheBase) {
    const std::array<Contribution<float>, 1> contributions{{
        {.source = "haste", .value = 1.2F},
    }};

    EXPECT_FLOAT_EQ(resolve_multiplicative<float>(10.0F, contributions), 12.0F);
}

TEST(ResolveMultiplicative, AZeroContributionCollapsesTheEffectiveValueToZero) {
    // Unlike Additive, a single Multiplicative contribution of exactly 0
    // (e.g. "rooted") zeroes out the effective value regardless of any other
    // active contribution - a single contribution can dominate every other
    // one this way, which Additive's arithmetic never allows.
    const std::array<Contribution<float>, 2> contributions{{
        {.source = "rooted", .value = 0.0F},
        {.source = "haste", .value = 1.2F},
    }};

    EXPECT_FLOAT_EQ(resolve_multiplicative<float>(10.0F, contributions), 0.0F);
}

TEST(ResolveOverride, BaseAloneWithNoContributionsIsUnchanged) {
    EXPECT_EQ(resolve_override<std::int32_t>(0, {}), 0);
}

TEST(ResolveOverride, MostRecentlyContributedValueWins) {
    // Reproduces spec §20's own Override example: "CurrentAnimation: Idle ->
    // Attack (combat state overrides)". The caller assembles contributions in
    // contribution order (the same convention resolve_additive/
    // resolve_multiplicative already rely on), so the last element in the
    // span is the most recently contributed value.
    const std::array<Contribution<std::string_view>, 2> contributions{{
        {.source = "idle", .value = "Idle"},
        {.source = "combat", .value = "Attack"},
    }};

    EXPECT_EQ(resolve_override<std::string_view>("Idle", contributions), "Attack");
}

TEST(ResolveOverride, ASingleContributionReplacesTheBaseOutright) {
    const std::array<Contribution<std::string_view>, 1> contributions{{
        {.source = "combat", .value = "Attack"},
    }};

    EXPECT_EQ(resolve_override<std::string_view>("Idle", contributions), "Attack");
}

TEST(ResolvePriorityOverride, BaseAloneWithNoContributionsIsUnchanged) {
    EXPECT_EQ(resolve_priority_override<std::string_view>("Default", {}), "Default");
}

TEST(ResolvePriorityOverride, HighestPriorityCandidateWinsAmongSeveral) {
    // Reproduces spec §20's own Priority Override example: "AnimationState:
    // Stunned > Weapon > Default".
    const std::array<Contribution<std::string_view>, 2> contributions{{
        {.source = "weapon", .value = "Weapon", .priority = 1},
        {.source = "stun", .value = "Stunned", .priority = 2},
    }};

    EXPECT_EQ(resolve_priority_override<std::string_view>("Default", contributions), "Stunned");
}

TEST(ResolvePriorityOverride, HigherPriorityContributionPreemptsImmediatelyWithoutRemovingTheLoser) {
    // Mirrors spec §20's "Continuous Re-resolution and Preemption" sequence
    // diagram exactly: backstab contributes StabAttack at High priority and
    // wins; movement then contributes Walk at a Higher priority and
    // *preempts* it - the effective value changes to Walk on the same
    // resolution, without StabAttack's contribution ever being withdrawn.
    // Removing the preempting contribution afterward hands resolution back
    // to the still-registered StabAttack contribution, proving it was never
    // deleted - only outranked.
    constexpr std::int32_t high_priority = 5;
    constexpr std::int32_t higher_priority = 10;
    const Contribution<std::string_view> backstab{
        .source = "backstab", .value = "StabAttack", .priority = high_priority};
    const Contribution<std::string_view> movement{
        .source = "movement", .value = "Walk", .priority = higher_priority};

    const std::array<Contribution<std::string_view>, 1> only_backstab{{backstab}};
    EXPECT_EQ(resolve_priority_override<std::string_view>("Default", only_backstab), "StabAttack");

    const std::array<Contribution<std::string_view>, 2> backstab_then_movement{{backstab, movement}};
    EXPECT_EQ(resolve_priority_override<std::string_view>("Default", backstab_then_movement), "Walk");

    // movement's contribution is removed (no longer part of the span) -
    // backstab's untouched contribution wins resolution again immediately.
    EXPECT_EQ(resolve_priority_override<std::string_view>("Default", only_backstab), "StabAttack");
}

TEST(ResolvePriorityOverride, TiesBreakInFavorOfTheLastContributionInSpanOrder) {
    const std::array<Contribution<std::string_view>, 2> contributions{{
        {.source = "first", .value = "First", .priority = 3},
        {.source = "second", .value = "Second", .priority = 3},
    }};

    EXPECT_EQ(resolve_priority_override<std::string_view>("Default", contributions), "Second");
}

TEST(ResolveSetUnion, BaseAloneWithNoContributionsIsUnchanged) {
    const std::vector<std::string_view> base{"HeavyArmor"};

    EXPECT_EQ(resolve_set_union<std::vector<std::string_view>>(base, {}), base);
}

TEST(ResolveSetUnion, MergesEveryContributionsCollection) {
    // Reproduces spec §20's own Set Union example: "Tags: [HeavyArmor] ∪
    // [Blessed] = [HeavyArmor, Blessed]".
    const std::vector<std::string_view> base{"HeavyArmor"};
    const std::array<Contribution<std::vector<std::string_view>>, 1> contributions{{
        {.source = "blessing", .value = {"Blessed"}},
    }};

    const std::vector<std::string_view> expected{"HeavyArmor", "Blessed"};
    EXPECT_EQ(resolve_set_union<std::vector<std::string_view>>(base, contributions), expected);
}

TEST(ResolveSetUnion, DuplicateElementsAcrossContributionsAreNotRepeated) {
    const std::vector<std::string_view> base{"HeavyArmor"};
    const std::array<Contribution<std::vector<std::string_view>>, 2> contributions{{
        {.source = "blessing", .value = {"Blessed"}},
        {.source = "second-blessing", .value = {"Blessed", "HeavyArmor"}},
    }};

    const std::vector<std::string_view> expected{"HeavyArmor", "Blessed"};
    EXPECT_EQ(resolve_set_union<std::vector<std::string_view>>(base, contributions), expected);
}

TEST(ResolveOrderedComposition, BaseAloneWithNoContributionsIsJustTheBaseLayer) {
    const std::vector<std::string_view> expected{"Skin"};

    EXPECT_EQ(resolve_ordered_composition<std::string_view>("Skin", {}), expected);
}

TEST(ResolveOrderedComposition, LayersInContributionOrderAfterTheBase) {
    // Reproduces spec §20's own Ordered Composition example: "MaterialLayers:
    // Skin -> Tattoo -> Armor -> DamageOverlay".
    const std::array<Contribution<std::string_view>, 3> contributions{{
        {.source = "tattoo", .value = "Tattoo"},
        {.source = "armor", .value = "Armor"},
        {.source = "damage", .value = "DamageOverlay"},
    }};

    const std::vector<std::string_view> expected{"Skin", "Tattoo", "Armor", "DamageOverlay"};
    EXPECT_EQ(resolve_ordered_composition<std::string_view>("Skin", contributions), expected);
}

TEST(ResolveWeightedComposition, BaseAloneWithNoContributionsIsUnchanged) {
    EXPECT_FLOAT_EQ(resolve_weighted_composition<float>(1.0F, {}), 1.0F);
}

TEST(ResolveWeightedComposition, BlendsContributionsProportionallyByWeight) {
    // Reproduces spec §20's own Weighted Composition example: "AnimationPose:
    // 70% Walk, 30% Run" - standing in Walk/Run's blend parameter as plain
    // floats (1.0 / 2.0) since the strategy itself is domain-agnostic.
    const std::array<Contribution<float>, 2> contributions{{
        {.source = "walk", .value = 1.0F, .weight = 0.7},
        {.source = "run", .value = 2.0F, .weight = 0.3},
    }};

    EXPECT_FLOAT_EQ(resolve_weighted_composition<float>(0.0F, contributions), 1.3F);
}

TEST(ResolveWeightedComposition, WeightsAreNormalizedRatherThanRequiredToSumToOne) {
    // The same 70/30 blend as above, expressed as un-normalized weights (7
    // and 3) - the strategy normalizes by the total weight rather than
    // assuming contributions already sum to 1.
    const std::array<Contribution<float>, 2> contributions{{
        {.source = "walk", .value = 1.0F, .weight = 7.0},
        {.source = "run", .value = 2.0F, .weight = 3.0},
    }};

    EXPECT_FLOAT_EQ(resolve_weighted_composition<float>(0.0F, contributions), 1.3F);
}

TEST(ResolveWeightedComposition, ZeroTotalWeightFallsBackToTheBase) {
    const std::array<Contribution<float>, 1> contributions{{
        {.source = "inert", .value = 5.0F, .weight = 0.0},
    }};

    EXPECT_FLOAT_EQ(resolve_weighted_composition<float>(2.0F, contributions), 2.0F);
}

TEST(ContributionPriority, DefaultsToZero) {
    const Contribution<std::int32_t> contribution{.source = "plate", .value = 50};

    EXPECT_EQ(contribution.priority, 0);
}

TEST(ContributionWeight, DefaultsToOne) {
    const Contribution<std::int32_t> contribution{.source = "plate", .value = 50};

    EXPECT_DOUBLE_EQ(contribution.weight, 1.0);
}

TEST(ContributionLifetime, DefaultsToPermanent) {
    // Every contribution added by this codebase so far (armor::add_contribution,
    // movement::add_speed_contribution) constructs via {.source = ..., .value =
    // ...} without ever naming lifetime - confirms that designated-initializer
    // shape still compiles and yields Permanent, unaffected by adding this field.
    const Contribution<std::int32_t> contribution{.source = "plate", .value = 50};

    EXPECT_EQ(contribution.lifetime, Lifetime::Permanent);
}

} // namespace
} // namespace atlas::runtime
