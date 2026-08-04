#include "atlas/session/session_id.hpp"

#include <gtest/gtest.h>
#include <unordered_set>

namespace atlas {
namespace {

TEST(SessionId, DefaultConstructsToNullSentinel) {
    constexpr SessionId id{};

    EXPECT_TRUE(id.is_null());
    EXPECT_EQ(id.high, 0U);
    EXPECT_EQ(id.low, 0U);
}

TEST(SessionId, NonZeroValueIsNotNull) {
    constexpr SessionId id{.high = 1, .low = 0};

    EXPECT_FALSE(id.is_null());
}

TEST(SessionId, EqualityComparesBothWords) {
    constexpr SessionId first{.high = 1, .low = 2};
    constexpr SessionId second{.high = 1, .low = 2};
    constexpr SessionId different_high{.high = 9, .low = 2};
    constexpr SessionId different_low{.high = 1, .low = 9};

    EXPECT_EQ(first, second);
    EXPECT_NE(first, different_high);
    EXPECT_NE(first, different_low);
}

TEST(SessionId, IsHashableForUnorderedContainers) {
    const SessionId first{.high = 1, .low = 2};
    const SessionId second{.high = 1, .low = 2};
    const SessionId third{.high = 3, .low = 4};

    std::unordered_set<SessionId> ids;
    ids.insert(first);
    ids.insert(second); // duplicate of `first`, should not grow the set
    ids.insert(third);

    EXPECT_EQ(ids.size(), 2U);
    EXPECT_TRUE(ids.contains(first));
    EXPECT_TRUE(ids.contains(third));
}

} // namespace
} // namespace atlas
