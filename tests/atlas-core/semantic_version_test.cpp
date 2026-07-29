#include "atlas/core/semantic_version.hpp"

#include <gtest/gtest.h>

namespace atlas::core {
namespace {

TEST(SemanticVersion, ParsesValidVersionString) {
    const auto version = SemanticVersion::parse("1.4.0");

    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->major_version(), 1U);
    EXPECT_EQ(version->minor_version(), 4U);
    EXPECT_EQ(version->patch_version(), 0U);
}

TEST(SemanticVersion, RejectsMalformedVersionStrings) {
    EXPECT_FALSE(SemanticVersion::parse("1.4").has_value());
    EXPECT_FALSE(SemanticVersion::parse("not-a-version").has_value());
    EXPECT_FALSE(SemanticVersion::parse("1.4.0-beta").has_value());
    EXPECT_FALSE(SemanticVersion::parse("1..0").has_value());
    EXPECT_FALSE(SemanticVersion::parse("").has_value());
}

TEST(SemanticVersion, RejectsNonNumericComponents) {
    EXPECT_FALSE(SemanticVersion::parse("x.4.0").has_value());
    EXPECT_FALSE(SemanticVersion::parse("1.x.0").has_value());
    EXPECT_FALSE(SemanticVersion::parse("1.4.x").has_value());
}

TEST(SemanticVersion, OrdersByPrecedence) {
    constexpr SemanticVersion older{1, 4, 0};
    constexpr SemanticVersion newer{1, 4, 1};

    EXPECT_LT(older, newer);
    EXPECT_LT((SemanticVersion{1, 4, 0}), (SemanticVersion{1, 5, 0})); // minor decides, major tied
    EXPECT_LT((SemanticVersion{1, 4, 0}), (SemanticVersion{2, 0, 0})); // major decides
    EXPECT_EQ(older, (SemanticVersion{1, 4, 0}));
}

} // namespace
} // namespace atlas::core
