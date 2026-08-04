#include "atlas/session/in_memory_session_store.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <unordered_set>

namespace atlas::session {
namespace {

constexpr std::size_t many_calls_sample_size = 10'000;

TEST(InMemorySessionStore, CreateSessionNeverReturnsTheNullSentinel) {
    InMemorySessionStore store;

    const SessionId id = store.create_session();

    EXPECT_FALSE(id.is_null());
}

TEST(InMemorySessionStore, CreateSessionReturnsDistinctIdsAcrossManyCalls) {
    InMemorySessionStore store;
    std::unordered_set<SessionId> seen;
    seen.reserve(many_calls_sample_size);

    for (std::size_t i = 0; i < many_calls_sample_size; ++i) {
        const SessionId id = store.create_session();
        const bool inserted = seen.insert(id).second;
        ASSERT_TRUE(inserted) << "duplicate SessionId produced on call " << i;
    }

    EXPECT_EQ(seen.size(), many_calls_sample_size);
}

TEST(InMemorySessionStore, TwoIndependentStoresDoNotOverlap) {
    InMemorySessionStore first_store;
    InMemorySessionStore second_store;
    std::unordered_set<SessionId> first_ids;
    first_ids.reserve(many_calls_sample_size);

    for (std::size_t i = 0; i < many_calls_sample_size; ++i) {
        first_ids.insert(first_store.create_session());
    }

    for (std::size_t i = 0; i < many_calls_sample_size; ++i) {
        const SessionId id = second_store.create_session();
        ASSERT_FALSE(first_ids.contains(id)) << "second store produced an id already seen in the first store";
    }
}

TEST(InMemorySessionStore, IsValidIsTrueForAJustCreatedSession) {
    InMemorySessionStore store;

    const SessionId id = store.create_session();

    EXPECT_TRUE(store.is_valid(id));
}

TEST(InMemorySessionStore, IsValidIsFalseForAnUnrelatedOrNeverCreatedSession) {
    InMemorySessionStore store;
    [[maybe_unused]] const SessionId unrelated =
        store.create_session(); // some unrelated activity in the store

    const SessionId never_created{.high = 0xDEAD'BEEF'DEAD'BEEFULL, .low = 0xFEED'FACE'FEED'FACEULL};

    EXPECT_FALSE(store.is_valid(never_created));
}

TEST(InMemorySessionStore, RevokeMakesAPreviouslyValidSessionInvalid) {
    InMemorySessionStore store;
    const SessionId id = store.create_session();
    ASSERT_TRUE(store.is_valid(id));

    store.revoke(id);

    EXPECT_FALSE(store.is_valid(id));
}

// Documented behavior (CLAUDE.md: "explicit, documented behavior for edge
// cases, never silently assumed"): revoking a session id the store never
// considered valid - whether because it was never created or because it was
// already revoked - is a silent no-op, never a thrown exception or crash.
// This matches std::unordered_set::erase's own "erasing a missing key is a
// no-op" behavior that InMemorySessionStore::revoke is built on, and keeps a
// caller from needing to track whether it already revoked a given session
// before calling revoke() again.
TEST(InMemorySessionStore, RevokingAnAlreadyRevokedSessionDoesNotThrow) {
    InMemorySessionStore store;
    const SessionId id = store.create_session();
    store.revoke(id);

    EXPECT_NO_THROW(store.revoke(id));
    EXPECT_FALSE(store.is_valid(id));
}

TEST(InMemorySessionStore, RevokingANeverCreatedSessionDoesNotThrow) {
    InMemorySessionStore store;
    const SessionId never_created{.high = 1, .low = 2};

    EXPECT_NO_THROW(store.revoke(never_created));
    EXPECT_FALSE(store.is_valid(never_created));
}

} // namespace
} // namespace atlas::session
