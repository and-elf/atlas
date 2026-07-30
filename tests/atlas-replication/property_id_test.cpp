#include "atlas/replication/property_id.hpp"

#include <gtest/gtest.h>

namespace atlas {
namespace {

TEST(PropertyId, FromNameIsDeterministic) {
    EXPECT_EQ(PropertyId::from_name("Health"), PropertyId::from_name("Health"));
}

TEST(PropertyId, DifferentNamesProduceDifferentIds) {
    EXPECT_NE(PropertyId::from_name("Health"), PropertyId::from_name("Armor"));
}

TEST(PropertyId, EmptyNameProducesTheNullId) {
    EXPECT_TRUE(PropertyId::from_name("").is_null());
}

TEST(PropertyId, DefaultConstructedIsNull) {
    EXPECT_TRUE(PropertyId{}.is_null());
}

TEST(PropertyId, FromNameNeverProducesTheNullIdForANonEmptyName) {
    EXPECT_FALSE(PropertyId::from_name("Health").is_null());
    EXPECT_FALSE(PropertyId::from_name("Armor").is_null());
    EXPECT_FALSE(PropertyId::from_name("CastSpeed").is_null());
}

// Known-answer test against FNV-1a 64 reference values (independently
// computed, not derived from this implementation) - mirrors
// atlas-resource's own ResourceId.MatchesFnv1a64ReferenceValues test.
TEST(PropertyId, MatchesFnv1a64ReferenceValues) {
    EXPECT_EQ(PropertyId::from_name("Health").value, 0xbaf0a7c417e388afULL);
    EXPECT_EQ(PropertyId::from_name("a").value, 0xaf63dc4c8601ec8cULL);
}

TEST(PropertyId, FromNameIsUsableAtCompileTime) {
    // A static_assert is itself the test - if from_name isn't constexpr,
    // this file fails to compile rather than a test failing at runtime.
    static_assert(PropertyId::from_name("").is_null());
    static_assert(!PropertyId::from_name("Health").is_null());
    static_assert(PropertyId::from_name("Health").value == 0xbaf0a7c417e388afULL);

    constexpr PropertyId compile_time_id = PropertyId::from_name("Health");
    EXPECT_EQ(compile_time_id, PropertyId::from_name("Health"));
}

TEST(PropertyId, IsOrderable) {
    // Only exercised because operator<=> is defaulted (spec-required for use
    // in ordered containers) - no ordering semantics are meaningful here
    // beyond "some total order exists", unlike EntityRef/ResourceId which
    // document the same thing.
    const auto a = PropertyId::from_name("Health");
    const auto b = PropertyId::from_name("Armor");
    EXPECT_TRUE((a < b) || (b < a) || (a == b));
}

} // namespace
} // namespace atlas
