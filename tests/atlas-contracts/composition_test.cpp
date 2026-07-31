#include "atlas/contracts/contract_concepts.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace atlas {
namespace {

// Reproduced from §20's own generated-contract example: a composed
// property carries a `composition` member naming its strategy, plus its
// base value as an ordinary field.
struct MovementSpeed {
    static constexpr auto composition = Composition::Multiplicative;

    float base = 7.0F;
};

// Health (§21) has no composition member at all - a plain, non-composed
// property. Reproduced locally rather than pulling in a generated fixture,
// mirroring how contract_concepts_test.cpp already reasons about plain
// PropertyContract-only shapes.
struct Health {
    std::int32_t current;
    std::int32_t maximum;
};

TEST(Composable, ComposedPropertySatisfiesComposable) {
    EXPECT_TRUE((Composable<MovementSpeed>));
}

TEST(Composable, ComposedPropertyStillSatisfiesPlainPropertyContract) {
    EXPECT_TRUE((PropertyContract<MovementSpeed>));
}

TEST(Composable, PlainPropertyWithoutACompositionMemberFailsComposable) {
    EXPECT_FALSE((Composable<Health>));
}

TEST(Composable, PlainPropertyStillSatisfiesPropertyContract) {
    EXPECT_TRUE((PropertyContract<Health>));
}

// Reproduced from a triggered property's generated-contract shape (spec §20,
// Triggered composition): a `static constexpr bool is_triggered` member
// marking that the property's value is meaningful only the tick it was
// written, plus its ordinary payload fields.
struct PositionChanged {
    static constexpr bool is_triggered = true;

    float new_x = 0.0F;
    float new_y = 0.0F;
};

TEST(Triggered, TriggeredPropertySatisfiesTriggered) {
    EXPECT_TRUE((Triggered<PositionChanged>));
}

TEST(Triggered, TriggeredPropertyStillSatisfiesPlainPropertyContract) {
    EXPECT_TRUE((PropertyContract<PositionChanged>));
}

TEST(Triggered, PlainPropertyWithoutAnIsTriggeredMemberFailsTriggered) {
    EXPECT_FALSE((Triggered<Health>));
}

TEST(Triggered, ComposedPropertyWithoutAnIsTriggeredMemberFailsTriggered) {
    EXPECT_FALSE((Triggered<MovementSpeed>));
}

TEST(Composition, EnumeratesEveryStrategyFromTheSpecTable) {
    // §20's Composition Strategies table, reproduced as a compile-time
    // proof every named strategy exists as an enumerator - not asserting
    // runtime behavior (only Additive has a working evaluator so far, see
    // atlas-runtime), just that the compile-time contract vocabulary is
    // complete.
    constexpr std::array strategies{
        Composition::Additive,
        Composition::Multiplicative,
        Composition::Override,
        Composition::PriorityOverride,
        Composition::SetUnion,
        Composition::OrderedComposition,
        Composition::WeightedComposition,
    };
    EXPECT_EQ(strategies.size(), 7U);
}

} // namespace
} // namespace atlas
