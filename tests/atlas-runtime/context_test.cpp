#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace atlas {
namespace {

runtime::Host make_host(bool has_authority) {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return runtime::Host{std::move(*sequence), has_authority};
}

// A tiny stand-in property, local to this test - Context itself never
// mentions a real capability's property by name (spec §2, Mechanism Over
// Meaning), so a made-up one is the right thing to test it against, not a
// real one borrowed from elsewhere.
struct Score {
    std::int32_t value;
};

struct ScoreChanged {
    std::int32_t new_value;
};

TEST(Context, HostAccessorReturnsTheWrappedHostsAuthority) {
    auto host = make_host(/*has_authority=*/true);
    Context ctx{host};

    EXPECT_TRUE(ctx.host().has_authority());
}

TEST(Context, GetWithNoStoreRegisteredThrows) {
    auto host = make_host(true);
    Context ctx{host};

    EXPECT_THROW((void)ctx.get<Score>(EntityRef{1, 0}), std::logic_error);
}

TEST(Context, GetOnAnEntityWithNoStoredValueReturnsNullopt) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    ctx.register_property_store(scores);

    EXPECT_FALSE(ctx.get<Score>(EntityRef{1, 0}).has_value());
}

TEST(Context, GetReturnsTheValueFromTheRegisteredStore) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    scores.set(EntityRef{1, 0}, Score{.value = 42});
    ctx.register_property_store(scores);

    const auto score = ctx.get<Score>(EntityRef{1, 0});
    ASSERT_TRUE(score.has_value());
    EXPECT_EQ(score->get().value, 42);
}

TEST(Context, GetReturnsAMutableReferenceThatWritesThroughToTheStore) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    scores.set(EntityRef{1, 0}, Score{.value = 1});
    ctx.register_property_store(scores);

    auto score = ctx.get<Score>(EntityRef{1, 0});
    ASSERT_TRUE(score.has_value());
    score->get().value = 99;

    EXPECT_EQ(scores.get(EntityRef{1, 0})->get().value, 99);
}

TEST(Context, SetWithNoStoreRegisteredThrows) {
    auto host = make_host(true);
    Context ctx{host};

    EXPECT_THROW(ctx.set<Score>(EntityRef{1, 0}, Score{.value = 1}), std::logic_error);
}

TEST(Context, SetCreatesAnEntryForAnEntityWithNoPriorValue) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    ctx.register_property_store(scores);

    ctx.set<Score>(EntityRef{1, 0}, Score{.value = 42});

    const auto score = ctx.get<Score>(EntityRef{1, 0});
    ASSERT_TRUE(score.has_value());
    EXPECT_EQ(score->get().value, 42);
}

TEST(Context, SetOverwritesAPreviouslyStoredValue) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    scores.set(EntityRef{1, 0}, Score{.value = 1});
    ctx.register_property_store(scores);

    ctx.set<Score>(EntityRef{1, 0}, Score{.value = 99});

    EXPECT_EQ(scores.get(EntityRef{1, 0})->get().value, 99);
}

TEST(Context, ResetPropertyWithNoStoreRegisteredThrows) {
    auto host = make_host(true);
    Context ctx{host};

    EXPECT_THROW(ctx.reset_property<Score>(), std::logic_error);
}

TEST(Context, ResetPropertyClearsEveryEntitysStoredValue) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    ctx.register_property_store(scores);
    ctx.set<Score>(EntityRef{1, 0}, Score{.value = 1});
    ctx.set<Score>(EntityRef{2, 0}, Score{.value = 2});

    ctx.reset_property<Score>();

    EXPECT_FALSE(ctx.get<Score>(EntityRef{1, 0}).has_value());
    EXPECT_FALSE(ctx.get<Score>(EntityRef{2, 0}).has_value());
}

// Reproduces the exact shape a triggered property needs (spec §20, Triggered
// composition): occurrence written via set<T>() (no pre-existing entry to
// mutate through, unlike a continuous property that already has a base
// value), read via the ordinary get<T>() every other property already uses,
// absent-this-tick observed as nullopt after reset_property<T>() runs.
TEST(Context, TriggeredPropertyRoundTripsThroughSetGetReset) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<ScoreChanged> occurrences;
    ctx.register_property_store(occurrences);

    EXPECT_FALSE(ctx.get<ScoreChanged>(EntityRef{1, 0}).has_value());

    ctx.set<ScoreChanged>(EntityRef{1, 0}, ScoreChanged{.new_value = 7});
    const auto occurred = ctx.get<ScoreChanged>(EntityRef{1, 0});
    ASSERT_TRUE(occurred.has_value());
    EXPECT_EQ(occurred->get().new_value, 7);

    ctx.reset_property<ScoreChanged>();
    EXPECT_FALSE(ctx.get<ScoreChanged>(EntityRef{1, 0}).has_value());
}

TEST(Context, RegisterTriggeredPropertyStoreStillSupportsGetAndSet) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<ScoreChanged> occurrences;
    ctx.register_triggered_property_store(occurrences);

    ctx.set<ScoreChanged>(EntityRef{1, 0}, ScoreChanged{.new_value = 7});

    const auto occurred = ctx.get<ScoreChanged>(EntityRef{1, 0});
    ASSERT_TRUE(occurred.has_value());
    EXPECT_EQ(occurred->get().new_value, 7);
}

TEST(Context, EndTickResetsARegisteredTriggeredStore) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<ScoreChanged> occurrences;
    ctx.register_triggered_property_store(occurrences);
    ctx.set<ScoreChanged>(EntityRef{1, 0}, ScoreChanged{.new_value = 7});

    ctx.end_tick();

    EXPECT_FALSE(ctx.get<ScoreChanged>(EntityRef{1, 0}).has_value());
}

TEST(Context, EndTickDoesNotAffectAnOrdinaryRegisteredStore) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<Score> scores;
    ctx.register_property_store(scores);
    ctx.set<Score>(EntityRef{1, 0}, Score{.value = 42});

    ctx.end_tick();

    const auto score = ctx.get<Score>(EntityRef{1, 0});
    ASSERT_TRUE(score.has_value());
    EXPECT_EQ(score->get().value, 42);
}

TEST(Context, EndTickWithNoTriggeredStoresRegisteredIsHarmless) {
    auto host = make_host(true);
    Context ctx{host};

    EXPECT_NO_THROW(ctx.end_tick());
}

TEST(Context, EndTickResetsEveryRegisteredTriggeredStore) {
    auto host = make_host(true);
    Context ctx{host};
    runtime::PropertyStore<ScoreChanged> occurrences;
    runtime::PropertyStore<Score> other_occurrences;
    ctx.register_triggered_property_store(occurrences);
    ctx.register_triggered_property_store(other_occurrences);
    ctx.set<ScoreChanged>(EntityRef{1, 0}, ScoreChanged{.new_value = 7});
    ctx.set<Score>(EntityRef{1, 0}, Score{.value = 3});

    ctx.end_tick();

    EXPECT_FALSE(ctx.get<ScoreChanged>(EntityRef{1, 0}).has_value());
    EXPECT_FALSE(ctx.get<Score>(EntityRef{1, 0}).has_value());
}

TEST(Context, PublishWithNoSubscribersIsHarmless) {
    auto host = make_host(true);
    Context ctx{host};

    EXPECT_NO_THROW(ctx.publish<ScoreChanged>(ScoreChanged{.new_value = 5}));
}

TEST(Context, PublishInvokesASubscribedHandlerWithTheEvent) {
    auto host = make_host(true);
    Context ctx{host};
    std::optional<std::int32_t> observed;
    ctx.subscribe<ScoreChanged>([&](const ScoreChanged& event) { observed = event.new_value; });

    ctx.publish<ScoreChanged>(ScoreChanged{.new_value = 7});

    ASSERT_TRUE(observed.has_value());
    EXPECT_EQ(*observed, 7);
}

TEST(Context, PublishInvokesEverySubscriberInRegistrationOrder) {
    auto host = make_host(true);
    Context ctx{host};
    std::vector<int> order;
    ctx.subscribe<ScoreChanged>([&](const ScoreChanged&) { order.push_back(1); });
    ctx.subscribe<ScoreChanged>([&](const ScoreChanged&) { order.push_back(2); });
    ctx.subscribe<ScoreChanged>([&](const ScoreChanged&) { order.push_back(3); });

    ctx.publish<ScoreChanged>(ScoreChanged{.new_value = 0});

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

} // namespace
} // namespace atlas
