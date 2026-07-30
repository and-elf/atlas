#include "atlas/runtime/property_composition.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>

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

} // namespace
} // namespace atlas::runtime
