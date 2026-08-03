#include "atlas/entity/entity_ref.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <vector>

#include "app.hpp"
#include "health/health.hpp"

namespace atlas::demo {
namespace {

// A minimal derived App exposing what's protected (host()/ctx(), on_tick()/
// stop_requested() overrides) so this file's tests can observe App's real
// behavior without touching real signals or real wall-clock sleep - every
// test below either constructs with an argv that never reaches App::run()'s
// unbounded (real-time-paced, Ctrl+C-only) branch, or overrides
// stop_requested() directly, since that branch is the one genuinely
// wall-clock/signal-dependent piece left (matching main.cpp's own prior
// "not unit tested" scope, now shrunk to just that one loop's pacing).
class TestApp : public App {
public:
    using App::App;

    [[nodiscard]] runtime::Host& host_for_test() { return host(); }
    [[nodiscard]] Context& ctx_for_test() { return ctx(); }
    [[nodiscard]] bool parsed_ok_for_test() const { return parsed_ok(); }

    std::vector<std::uint64_t> ticks_observed;
    bool force_stop_immediately = false;

protected:
    void on_tick(std::uint64_t tick) override { ticks_observed.push_back(tick); }

    [[nodiscard]] bool stop_requested() const override {
        return force_stop_immediately || App::stop_requested();
    }
};

// Builds a real argv-shaped array (argv[argc] must be a null pointer, the
// same contract main()'s own argv has) from a fixed list of C strings - App
// takes int argc/char** argv exactly like main() does, so tests construct it
// the same way a real process would.
class Argv {
public:
    explicit Argv(std::initializer_list<const char*> args) : args_(args.begin(), args.end()) {
        pointers_.reserve(args_.size() + 1);
        for (auto& arg : args_) {
            pointers_.push_back(arg.data());
        }
        pointers_.push_back(nullptr);
    }

    [[nodiscard]] int argc() const { return static_cast<int>(args_.size()); }
    [[nodiscard]] char** argv() { return pointers_.data(); }

private:
    std::vector<std::string> args_;
    std::vector<char*> pointers_;
};

TEST(App, ConstructionWithNoArgsParsesOkAndComposesAllDemoCapabilities) {
    Argv args{"demo-host"};
    TestApp app(args.argc(), args.argv());

    EXPECT_TRUE(app.parsed_ok_for_test());
    // A real, composed PropertyStore is reachable through ctx() - proves
    // register_property_stores actually ran during construction, the same
    // composition mechanism SimulatedHost wraps for tests.
    const EntityRef entity = app.host_for_test().create_entity();
    EXPECT_NO_THROW(
        app.ctx_for_test().set<health::Health>(entity, health::Health{.current = 1, .maximum = 1}));
}

TEST(App, ConstructionWithAnUnrecognizedArgFailsToParse) {
    Argv args{"demo-host", "--unknown"};
    TestApp app(args.argc(), args.argv());

    EXPECT_FALSE(app.parsed_ok_for_test());
}

TEST(App, RunReturnsNonZeroAndNeverTicksWhenArgvFailedToParse) {
    Argv args{"demo-host", "--unknown"};
    TestApp app(args.argc(), args.argv());

    EXPECT_EQ(app.run(), 2);
    EXPECT_TRUE(app.ticks_observed.empty());
}

TEST(App, RunWithBoundedTicksInvokesOnTickOnceForEachTickWithA1BasedCounterAndReturnsZero) {
    Argv args{"demo-host", "--ticks", "5"};
    TestApp app(args.argc(), args.argv());

    EXPECT_EQ(app.run(), 0);
    EXPECT_EQ(app.ticks_observed, (std::vector<std::uint64_t>{1, 2, 3, 4, 5}));
}

TEST(App, RunWithZeroBoundedTicksReturnsZeroWithoutInvokingOnTick) {
    Argv args{"demo-host", "--ticks", "0"};
    TestApp app(args.argc(), args.argv());

    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.ticks_observed.empty());
}

TEST(App, StopRequestedCanBeOverriddenByADerivedClassWithoutTouchingSignals) {
    // No --ticks bound at all - only an immediately-true stop_requested()
    // override (never real SIGINT/SIGTERM) keeps this from looping forever,
    // proving the override hook the PR #185 review comment asked for
    // ("optional support for overriding") actually works.
    Argv args{"demo-host"};
    TestApp app(args.argc(), args.argv());
    app.force_stop_immediately = true;

    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.ticks_observed.empty());
}

} // namespace
} // namespace atlas::demo
