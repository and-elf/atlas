#include "atlas/request/dispatch.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
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

// issue #71 part 1: pre_tick is the hook a caller uses to turn this tick's
// polled input into a dispatched request *before* advance_tick resolves it -
// on_tick (which fires after) is too late for that. Appended as a trailing
// parameter after on_tick, not inserted before it, so every existing
// positional call above (which passes its callback as the 4th argument)
// keeps meaning on_tick, not pre_tick.
TEST(HostLoop, RunTicksInvokesPreTickBeforeOnTickEachIteration) {
    testing::SimulatedHost server{/*has_authority=*/true};

    std::vector<std::string> order;
    run_ticks(
        server.host,
        server.ctx,
        2,
        [&order](std::uint64_t) { order.push_back("on_tick"); },
        [&order](std::uint64_t) { order.push_back("pre_tick"); });

    EXPECT_EQ(order, (std::vector<std::string>{"pre_tick", "on_tick", "pre_tick", "on_tick"}));
}

TEST(HostLoop, RunTicksWithoutAPreTickCallbackDoesNotThrow) {
    testing::SimulatedHost server{/*has_authority=*/true};

    EXPECT_NO_THROW(run_ticks(server.host, server.ctx, 2, [](std::uint64_t) {}));
}

TEST(HostLoop, PreTickRunsBeforeAdvanceTickSoADispatchedRequestIsVisibleInOnTickThisSameTick) {
    testing::SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 0.0F, .y = 0.0F});
    server.movement_speed_store.set(target, movement::MovementSpeed{.base = 0.0F});
    movement::set_base_speed(server.ctx, server.movement_speed_contributions, target, 6.0F);

    request::Dispatcher<movement::Move> dispatcher;
    dispatcher.register_handler(movement::on_move);

    float observed_x = -1.0F;
    run_ticks(
        server.host,
        server.ctx,
        1,
        [&](std::uint64_t) { observed_x = server.ctx.get<movement::Position>(target)->get().x; },
        [&](std::uint64_t) {
            const RequestResult result = dispatcher.dispatch(
                server.ctx,
                movement::Move{
                    .target = target, .direction_x = 1.0F, .direction_y = 0.0F, .delta_ticks = 60});
            ASSERT_TRUE(result.accepted);
        });

    // 60 ticks at 60 ticks_per_second is 1 second; at speed 6.0 that's 6.0
    // units - already reflected in on_tick's read, proving pre_tick's
    // dispatch resolved before advance_tick ran this same tick.
    EXPECT_FLOAT_EQ(observed_x, 6.0F);
}

} // namespace
} // namespace atlas::demo
