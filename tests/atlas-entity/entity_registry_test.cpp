#include "atlas/entity/entity_registry.hpp"

#include <gtest/gtest.h>

namespace atlas::entity {
namespace {

TEST(EntityRegistry, CreateReturnsDistinctLiveEntities) {
    EntityRegistry registry;

    const auto first = registry.create();
    const auto second = registry.create();

    EXPECT_NE(first, second);
    EXPECT_TRUE(registry.is_alive(first));
    EXPECT_TRUE(registry.is_alive(second));
}

TEST(EntityRegistry, DestroyMakesEntityNoLongerAlive) {
    EntityRegistry registry;
    const auto entity = registry.create();

    EXPECT_TRUE(registry.destroy(entity));
    EXPECT_FALSE(registry.is_alive(entity));
}

TEST(EntityRegistry, DestroyingAnAlreadyDestroyedEntityFails) {
    EntityRegistry registry;
    const auto entity = registry.create();
    ASSERT_TRUE(registry.destroy(entity));

    EXPECT_FALSE(registry.destroy(entity));
}

TEST(EntityRegistry, DestroyingAnUnknownEntityFails) {
    EntityRegistry registry;

    EXPECT_FALSE(registry.destroy(EntityRef{42, 0}));
}

TEST(EntityRegistry, RecycledIndexGetsANewGenerationAndOldRefStaysDead) {
    EntityRegistry registry;
    const auto first = registry.create();
    ASSERT_TRUE(registry.destroy(first));

    const auto second = registry.create();

    EXPECT_EQ(first.index, second.index);           // slot reused
    EXPECT_NE(first.generation, second.generation); // but distinguishable
    EXPECT_NE(first, second);
    EXPECT_FALSE(registry.is_alive(first));
    EXPECT_TRUE(registry.is_alive(second));
}

TEST(EntityRegistry, NullEntityRefIsNeverAlive) {
    EntityRegistry registry;

    EXPECT_FALSE(registry.is_alive(EntityRef{}));
}

TEST(EntityRegistry, CreateRecordsEntityAsCreatedSinceLastPoll) {
    EntityRegistry registry;

    const auto entity = registry.create();

    ASSERT_EQ(registry.created_since_last_poll().size(), 1U);
    EXPECT_EQ(registry.created_since_last_poll()[0], entity);
    EXPECT_TRUE(registry.destroyed_since_last_poll().empty());
}

TEST(EntityRegistry, MultipleCreatesAccumulateInOrderUntilCleared) {
    EntityRegistry registry;

    const auto first = registry.create();
    const auto second = registry.create();

    ASSERT_EQ(registry.created_since_last_poll().size(), 2U);
    EXPECT_EQ(registry.created_since_last_poll()[0], first);
    EXPECT_EQ(registry.created_since_last_poll()[1], second);
}

TEST(EntityRegistry, SuccessfulDestroyRecordsEntityAsDestroyedSinceLastPoll) {
    EntityRegistry registry;
    const auto entity = registry.create();
    registry.clear_lifecycle_events();

    ASSERT_TRUE(registry.destroy(entity));

    ASSERT_EQ(registry.destroyed_since_last_poll().size(), 1U);
    EXPECT_EQ(registry.destroyed_since_last_poll()[0], entity);
    EXPECT_TRUE(registry.created_since_last_poll().empty());
}

TEST(EntityRegistry, FailedDestroyOfAlreadyDestroyedEntityDoesNotRecordADuplicateEvent) {
    EntityRegistry registry;
    const auto entity = registry.create();
    ASSERT_TRUE(registry.destroy(entity));
    registry.clear_lifecycle_events();

    EXPECT_FALSE(registry.destroy(entity));
    EXPECT_TRUE(registry.destroyed_since_last_poll().empty());
}

TEST(EntityRegistry, FailedDestroyOfUnknownEntityDoesNotRecordAnEvent) {
    EntityRegistry registry;

    EXPECT_FALSE(registry.destroy(EntityRef{42, 0}));
    EXPECT_TRUE(registry.destroyed_since_last_poll().empty());
}

TEST(EntityRegistry, ClearLifecycleEventsEmptiesBothCreatedAndDestroyedLists) {
    EntityRegistry registry;
    const auto entity = registry.create();
    ASSERT_TRUE(registry.destroy(entity));
    ASSERT_FALSE(registry.created_since_last_poll().empty());
    ASSERT_FALSE(registry.destroyed_since_last_poll().empty());

    registry.clear_lifecycle_events();

    EXPECT_TRUE(registry.created_since_last_poll().empty());
    EXPECT_TRUE(registry.destroyed_since_last_poll().empty());
}

TEST(EntityRegistry, LifecycleEventsForOneEntityCanBeObservedBeforeClearing) {
    // A recycled slot's destroy-then-create both land in the same poll
    // window when the caller hasn't cleared in between - both events must
    // be observable, not just the latest one.
    EntityRegistry registry;
    const auto first = registry.create();
    registry.clear_lifecycle_events();

    ASSERT_TRUE(registry.destroy(first));
    const auto second = registry.create();

    ASSERT_EQ(registry.destroyed_since_last_poll().size(), 1U);
    EXPECT_EQ(registry.destroyed_since_last_poll()[0], first);
    ASSERT_EQ(registry.created_since_last_poll().size(), 1U);
    EXPECT_EQ(registry.created_since_last_poll()[0], second);
}

TEST(EntityRef, DefaultConstructedIsNull) {
    EXPECT_TRUE(EntityRef{}.is_null());
    EXPECT_FALSE(EntityRef(0, 0).is_null());
}

TEST(EntityRef, EqualityComparesIndexAndGeneration) {
    EXPECT_EQ((EntityRef{1, 0}), (EntityRef{1, 0}));
    EXPECT_NE((EntityRef{1, 0}), (EntityRef{1, 1}));
    EXPECT_NE((EntityRef{1, 0}), (EntityRef{2, 0}));
}

TEST(EntityRef, HashIsConsistentWithEquality) {
    const std::hash<EntityRef> hasher;

    // Equal refs must hash equal (the std::unordered_map/set requirement
    // PropertyStore, atlas-runtime, relies on).
    EXPECT_EQ(hasher(EntityRef{1, 0}), hasher(EntityRef{1, 0}));

    // Not a correctness requirement (hash collisions are always legal), but
    // a real distinctness check for a hash that combines both fields rather
    // than only one of them - if this ever collided, index or generation
    // would silently be getting dropped from the hash.
    EXPECT_NE(hasher(EntityRef{1, 0}), hasher(EntityRef{1, 1}));
    EXPECT_NE(hasher(EntityRef{1, 0}), hasher(EntityRef{2, 0}));
}

} // namespace
} // namespace atlas::entity
