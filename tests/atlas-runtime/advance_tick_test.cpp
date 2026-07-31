#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <cstdint>
#include <gtest/gtest.h>

namespace atlas {
namespace {

runtime::Host make_host(bool has_authority) {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return runtime::Host{std::move(*sequence), has_authority};
}

// A tiny stand-in triggered property, local to this test - see
// context_test.cpp's own ScoreChanged for the same reasoning (Context/
// advance_tick never mention a real capability's property by name, spec §2).
struct ScoreChanged {
    std::int32_t new_value;
};

struct Score {
    std::int32_t value;
};

TEST(AdvanceTick, RunsTheHostsScheduledJobs) {
    auto host = make_host(true);
    Context ctx{host};
    bool ran = false;
    ASSERT_TRUE(host.schedule(stage::StageId{"Simulation"}, [&] { ran = true; }));

    advance_tick(host, ctx);

    EXPECT_TRUE(ran);
}

// The whole point of issue #38: a triggered property written by a job that
// ran this tick reads back as nullopt once advance_tick returns - the
// tick-boundary reset happens automatically, not via a caller remembering to
// call ctx.reset_property<T>() by hand for every triggered type.
TEST(AdvanceTick, ResetsATriggeredPropertyWrittenDuringTheTick) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<ScoreChanged> occurrences;
    ctx.register_triggered_property_store(occurrences);
    ASSERT_TRUE(host.schedule(stage::StageId{"Simulation"},
                              [&] { ctx.set<ScoreChanged>(EntityRef{1, 0}, ScoreChanged{.new_value = 7}); }));

    advance_tick(host, ctx);

    EXPECT_FALSE(ctx.get<ScoreChanged>(EntityRef{1, 0}).has_value());
}

TEST(AdvanceTick, OrdinaryPropertiesSurviveAcrossTicks) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    ctx.register_property_store(scores);
    ctx.set<Score>(EntityRef{1, 0}, Score{.value = 42});

    advance_tick(host, ctx);
    advance_tick(host, ctx);

    const auto score = ctx.get<Score>(EntityRef{1, 0});
    ASSERT_TRUE(score.has_value());
    EXPECT_EQ(score->get().value, 42);
}

TEST(AdvanceTick, ATriggeredPropertyCanBeObservedTheSameTickItWasWrittenBeforeTheNextAdvance) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<ScoreChanged> occurrences;
    ctx.register_triggered_property_store(occurrences);
    ctx.set<ScoreChanged>(EntityRef{1, 0}, ScoreChanged{.new_value = 3});

    // Observed here, mid-tick, before advance_tick's end-of-tick reset runs -
    // exactly what a consumer reading a triggered property during its own
    // scheduled turn (spec §20, Triggered composition) needs.
    const auto occurred = ctx.get<ScoreChanged>(EntityRef{1, 0});
    ASSERT_TRUE(occurred.has_value());
    EXPECT_EQ(occurred->get().new_value, 3);

    advance_tick(host, ctx);

    EXPECT_FALSE(ctx.get<ScoreChanged>(EntityRef{1, 0}).has_value());
}

} // namespace
} // namespace atlas
