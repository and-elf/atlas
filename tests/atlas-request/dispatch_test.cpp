#include "atlas/request/dispatch.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::request {
namespace {

// A tiny stand-in request, local to this test - Dispatcher itself never
// mentions a real capability's request by name (spec §2, Mechanism Over
// Meaning), so a made-up one is the right thing to test it against.
struct Ping {
    std::int32_t value;
};

runtime::Host make_host(bool has_authority) {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return runtime::Host{std::move(*sequence), has_authority};
}

TEST(Dispatcher, DispatchWithNoHandlerRegisteredThrows) {
    auto host = make_host(true);
    Context ctx{host};
    Dispatcher<Ping> dispatcher;

    EXPECT_THROW((void)dispatcher.dispatch(ctx, Ping{.value = 1}), std::logic_error);
}

TEST(Dispatcher, DispatchRoutesToTheRegisteredHandler) {
    auto host = make_host(true);
    Context ctx{host};
    Dispatcher<Ping> dispatcher;
    bool handler_called = false;
    dispatcher.register_handler([&](Context&, const Ping& ping) -> RequestResult {
        handler_called = true;
        return accept(ping);
    });

    const RequestResult result = dispatcher.dispatch(ctx, Ping{.value = 42});

    EXPECT_TRUE(handler_called);
    EXPECT_TRUE(result.accepted);
}

TEST(Dispatcher, HandlerReceivesTheSameContextAndRequestPassedToDispatch) {
    auto host = make_host(true);
    Context ctx{host};
    Dispatcher<Ping> dispatcher;
    std::int32_t observed_value = 0;
    bool observed_authority = false;
    dispatcher.register_handler([&](Context& handler_ctx, const Ping& ping) -> RequestResult {
        observed_value = ping.value;
        observed_authority = handler_ctx.host().has_authority();
        return accept(ping);
    });

    (void)dispatcher.dispatch(ctx, Ping{.value = 7});

    EXPECT_EQ(observed_value, 7);
    EXPECT_TRUE(observed_authority);
}

TEST(Dispatcher, RegisteringASecondHandlerReplacesTheFirst) {
    auto host = make_host(true);
    Context ctx{host};
    Dispatcher<Ping> dispatcher;
    dispatcher.register_handler(
        [](Context&, const Ping& ping) -> RequestResult { return reject(ping, "first handler"); });
    dispatcher.register_handler([](Context&, const Ping& ping) -> RequestResult { return accept(ping); });

    const RequestResult result = dispatcher.dispatch(ctx, Ping{.value = 1});

    EXPECT_TRUE(result.accepted);
}

TEST(Dispatcher, HandlerCanRejectThroughTheOrdinaryAcceptRejectPath) {
    auto host = make_host(false);
    Context ctx{host};
    Dispatcher<Ping> dispatcher;
    dispatcher.register_handler([](Context& handler_ctx, const Ping& ping) -> RequestResult {
        if (!handler_ctx.host().has_authority()) {
            return reject(ping, "not authoritative");
        }
        return accept(ping);
    });

    const RequestResult result = dispatcher.dispatch(ctx, Ping{.value = 1});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

} // namespace
} // namespace atlas::request
