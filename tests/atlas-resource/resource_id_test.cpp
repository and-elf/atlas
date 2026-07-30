#include "atlas/resource/resource_id.hpp"

#include <gtest/gtest.h>
#include <unordered_set>

namespace atlas {
namespace {

TEST(ResourceId, DefaultConstructedIsNull) {
    EXPECT_TRUE(ResourceId{}.is_null());
}

TEST(ResourceId, FromEmptyNameIsNull) {
    EXPECT_TRUE(ResourceId::from_name("").is_null());
}

TEST(ResourceId, FromNonEmptyNameIsNeverNull) {
    EXPECT_FALSE(ResourceId::from_name("characters/hero/mesh").is_null());
}

TEST(ResourceId, FromNameIsDeterministicAcrossCalls) {
    const auto first = ResourceId::from_name("characters/hero/mesh");
    const auto second = ResourceId::from_name("characters/hero/mesh");

    EXPECT_EQ(first, second);
}

TEST(ResourceId, DifferentNamesProduceDifferentIds) {
    const auto mesh = ResourceId::from_name("characters/hero/mesh");
    const auto mesh2 = ResourceId::from_name("characters/hero/mesh2");

    EXPECT_NE(mesh, mesh2);
}

// Known-answer test against FNV-1a 64 reference values (independently
// computed, not derived from this implementation) — pins the exact
// algorithm rather than only checking internal self-consistency, per
// CLAUDE.md's guidance that bit-exact determinism is itself testable.
TEST(ResourceId, MatchesFnv1a64ReferenceValues) {
    EXPECT_EQ(ResourceId::from_name("a").value, 0xaf63dc4c8601ec8cULL);
    EXPECT_EQ(ResourceId::from_name("atlas").value, 0x890c8c80c57b984cULL);
    EXPECT_EQ(ResourceId::from_name("characters/hero/mesh").value, 0xfe4526d7c0a3b03aULL);
}

TEST(ResourceId, OrdersByUnderlyingValue) {
    constexpr ResourceId lower{1};
    constexpr ResourceId higher{2};

    EXPECT_LT(lower, higher);
    EXPECT_EQ(lower, (ResourceId{1}));
}

TEST(ResourceId, FromNameIsUsableAtCompileTime) {
    // A static_assert is itself the test - if from_name isn't constexpr,
    // this file fails to compile rather than a test failing at runtime.
    static_assert(ResourceId::from_name("").is_null());
    static_assert(!ResourceId::from_name("characters/hero/mesh").is_null());
    static_assert(ResourceId::from_name("a").value == 0xaf63dc4c8601ec8cULL);

    constexpr ResourceId compile_time_id = ResourceId::from_name("characters/hero/mesh");
    EXPECT_EQ(compile_time_id, ResourceId::from_name("characters/hero/mesh"));
}

TEST(ResourceId, UsableAsUnorderedContainerKey) {
    const auto mesh = ResourceId::from_name("characters/hero/mesh");
    const auto texture = ResourceId::from_name("characters/hero/texture");

    std::unordered_set<ResourceId> ids{mesh, texture, mesh};

    EXPECT_EQ(ids.size(), 2U);
    EXPECT_EQ(ids.count(mesh), 1U);
}

} // namespace
} // namespace atlas
