#include <array>
#include <gtest/gtest.h>

#include "presentation_sync.hpp"

namespace atlas::demo {
namespace {

TEST(PresentationSync, SyncsTransformFromPositionForEachTrackedEntityThatHasOne) {
    runtime::PropertyStore<movement::Position> positions;
    runtime::PropertyStore<render::Transform> transforms;
    const EntityRef entity{.index = 1, .generation = 0};
    positions.set(entity, movement::Position{.x = 3.0F, .y = -2.0F});

    const std::array<EntityRef, 1> entities{entity};
    sync_transforms(entities, positions, transforms);

    const auto transform = transforms.get(entity);
    ASSERT_TRUE(transform.has_value());
    EXPECT_FLOAT_EQ(transform->get().position.x, 3.0F);
    EXPECT_FLOAT_EQ(transform->get().position.y, -2.0F);
    EXPECT_FLOAT_EQ(transform->get().position.z, 0.0F);
    // Rotation/scale are left at Transform's own defaults (identity
    // rotation, unit scale) - this function has no opinion on either.
    EXPECT_FLOAT_EQ(transform->get().rotation.w, 1.0F);
    EXPECT_FLOAT_EQ(transform->get().scale.x, 1.0F);
}

TEST(PresentationSync, SkipsAnEntityWithNoStoredPosition) {
    runtime::PropertyStore<movement::Position> positions;
    runtime::PropertyStore<render::Transform> transforms;
    const EntityRef entity{.index = 1, .generation = 0}; // no Position seeded

    const std::array<EntityRef, 1> entities{entity};
    sync_transforms(entities, positions, transforms);

    EXPECT_FALSE(transforms.get(entity).has_value());
}

TEST(PresentationSync, OverwritesAPreviouslySyncedTransformOnTheNextCall) {
    runtime::PropertyStore<movement::Position> positions;
    runtime::PropertyStore<render::Transform> transforms;
    const EntityRef entity{.index = 1, .generation = 0};
    const std::array<EntityRef, 1> entities{entity};

    positions.set(entity, movement::Position{.x = 0.0F, .y = 0.0F});
    sync_transforms(entities, positions, transforms);

    positions.set(entity, movement::Position{.x = 5.0F, .y = 1.0F});
    sync_transforms(entities, positions, transforms);

    EXPECT_FLOAT_EQ(transforms.get(entity)->get().position.x, 5.0F);
    EXPECT_FLOAT_EQ(transforms.get(entity)->get().position.y, 1.0F);
}

} // namespace
} // namespace atlas::demo
