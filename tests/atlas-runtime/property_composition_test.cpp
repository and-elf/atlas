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

} // namespace
} // namespace atlas::runtime
