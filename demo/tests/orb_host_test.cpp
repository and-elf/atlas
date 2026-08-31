#include "atlas/core/time.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "movement/movement.hpp"
#include "orb_host.hpp"

namespace atlas::demo {
namespace {

TEST(OrbHost, SpawnOrbSeedsAZeroedPositionAndAPositiveEffectiveSpeed) {
    OrbApp app{/*has_authority=*/true};

    const EntityRef orb = spawn_orb(app);

    const auto position = app.ctx.get<movement::Position>(orb);
    ASSERT_TRUE(position.has_value());
    EXPECT_FLOAT_EQ(position->get().x, 0.0F);
    EXPECT_FLOAT_EQ(position->get().y, 0.0F);

    const auto speed = app.ctx.get<movement::MovementSpeed>(orb);
    ASSERT_TRUE(speed.has_value());
    EXPECT_GT(speed->get().base, 0.0F);
}

TEST(OrbHost, SpawnOrbSeedsARenderableMatchingTheFirstPaletteEntry) {
    OrbApp app{/*has_authority=*/true};

    const EntityRef orb = spawn_orb(app);

    const auto renderable = app.renderable_store.get(orb);
    ASSERT_TRUE(renderable.has_value());
    EXPECT_EQ(renderable->get().material, ResourceId::from_name(kOrbMaterialPalette[0]));
}

TEST(OrbHost, MaterialPaletteEntriesAreAllDistinctResourceIds) {
    std::vector<ResourceId> ids;
    ids.reserve(kOrbMaterialPalette.size());
    for (const auto& name : kOrbMaterialPalette) {
        ids.push_back(ResourceId::from_name(name));
    }

    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t j = i + 1; j < ids.size(); ++j) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST(RunPaced, BoundedTickLimitInvokesTheCallbackExactlyThatManyTimesWithA1BasedCounter) {
    OrbApp app{/*has_authority=*/true};

    std::vector<std::uint64_t> observed;
    run_paced(app, /*tick_limit=*/3, [&observed](std::uint64_t tick) { observed.push_back(tick); });

    EXPECT_EQ(observed, (std::vector<std::uint64_t>{1, 2, 3}));
}

TEST(RunPaced, BoundedZeroTickLimitInvokesTheCallbackNoTimes) {
    OrbApp app{/*has_authority=*/true};

    std::vector<std::uint64_t> observed;
    run_paced(app, /*tick_limit=*/0, [&observed](std::uint64_t tick) { observed.push_back(tick); });

    EXPECT_TRUE(observed.empty());
}

} // namespace
} // namespace atlas::demo
