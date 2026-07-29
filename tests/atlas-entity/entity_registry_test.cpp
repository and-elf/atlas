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

    EXPECT_EQ(first.index(), second.index());           // slot reused
    EXPECT_NE(first.generation(), second.generation()); // but distinguishable
    EXPECT_NE(first, second);
    EXPECT_FALSE(registry.is_alive(first));
    EXPECT_TRUE(registry.is_alive(second));
}

TEST(EntityRegistry, NullEntityRefIsNeverAlive) {
    EntityRegistry registry;

    EXPECT_FALSE(registry.is_alive(EntityRef{}));
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

} // namespace
} // namespace atlas::entity
