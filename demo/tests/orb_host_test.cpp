#include "atlas/core/time.hpp"

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
