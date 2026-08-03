#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "host_loop.hpp"
#include "movement/movement.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

// run_ticks (issue #70) is this codebase's first exerciser of
// Host::run_tick()/Context::end_tick() end to end for demo's composed
// capabilities - every demo/tests/*.cpp scenario elsewhere drives time via a
// delta_ticks field on an individual request instead (see demo/README.md).
// Reuses testing::SimulatedHost purely for its already-composed
// Host+Context+PropertyStore wiring, exactly like every other demo/tests/
// file - this is not testing SimulatedHost itself.

TEST(HostLoop, RunTicksInvokesTheCallbackOnceForEachTickWithA1BasedCounter) {
    testing::SimulatedHost server{/*has_authority=*/true};

    std::vector<std::uint64_t> observed;
    run_ticks(server.host, server.ctx, 3, [&observed](std::uint64_t tick) { observed.push_back(tick); });

    EXPECT_EQ(observed, (std::vector<std::uint64_t>{1, 2, 3}));
}

TEST(HostLoop, RunTicksWithoutACallbackDoesNotThrow) {
    testing::SimulatedHost server{/*has_authority=*/true};

    EXPECT_NO_THROW(run_ticks(server.host, server.ctx, 2));
}

TEST(HostLoop, RunTicksWithZeroTickCountInvokesTheCallbackNoTimes) {
    testing::SimulatedHost server{/*has_authority=*/true};

    std::vector<std::uint64_t> observed;
    run_ticks(server.host, server.ctx, 0, [&observed](std::uint64_t tick) { observed.push_back(tick); });

    EXPECT_TRUE(observed.empty());
}

TEST(HostLoop, RunTicksResetsATriggeredPropertyAtEachTickBoundary) {
    testing::SimulatedHost server{/*has_authority=*/true};
    const EntityRef entity = server.host.create_entity();

    server.ctx.set<movement::PositionChanged>(entity,
                                              movement::PositionChanged{.new_x = 1.0F, .new_y = 2.0F});
    ASSERT_TRUE(server.ctx.get<movement::PositionChanged>(entity).has_value());

    run_ticks(server.host, server.ctx, 1);

    // advance_tick's Context::end_tick() clears every triggered property
    // once per tick - proves run_ticks actually reaches that reset, not just
    // Host::run_tick() in isolation.
    EXPECT_FALSE(server.ctx.get<movement::PositionChanged>(entity).has_value());
}

} // namespace
} // namespace atlas::demo
