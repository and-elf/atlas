#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace atlas::runtime {
namespace {

stage::StageSequence make_sequence() {
    auto sequence = stage::StageSequence::create(
        {stage::StageId{"Input"}, stage::StageId{"Simulation"}, stage::StageId{"Presentation"}});
    return std::move(*sequence);
}

TEST(Host, HasAuthorityReflectsWhatTheHostWasConstructedWith) {
    const Host server_host{make_sequence(), /*has_authority=*/true};
    const Host client_host{make_sequence(), /*has_authority=*/false};

    EXPECT_TRUE(server_host.has_authority());
    EXPECT_FALSE(client_host.has_authority());
}

TEST(Host, CreatedEntityIsAlive) {
    Host host{make_sequence(), true};

    const EntityRef ref = host.create_entity();

    EXPECT_TRUE(host.is_entity_alive(ref));
}

TEST(Host, DestroyedEntityIsNoLongerAlive) {
    Host host{make_sequence(), true};
    const EntityRef ref = host.create_entity();

    const bool destroyed = host.destroy_entity(ref);

    EXPECT_TRUE(destroyed);
    EXPECT_FALSE(host.is_entity_alive(ref));
}

TEST(Host, DestroyingAnUnknownEntityFails) {
    Host host{make_sequence(), true};

    const bool destroyed = host.destroy_entity(EntityRef{});

    EXPECT_FALSE(destroyed);
}

TEST(Host, IsEntityAliveIsFalseForNeverCreatedRef) {
    const Host host{make_sequence(), true};

    EXPECT_FALSE(host.is_entity_alive(EntityRef{.index = 0, .generation = 0}));
}

TEST(Host, ScheduleRejectsStageNotInSequence) {
    Host host{make_sequence(), true};

    const bool accepted = host.schedule(stage::StageId{"NotAStage"}, [] {});

    EXPECT_FALSE(accepted);
}

TEST(Host, ScheduleAcceptsStageInSequence) {
    Host host{make_sequence(), true};

    const bool accepted = host.schedule(stage::StageId{"Simulation"}, [] {});

    EXPECT_TRUE(accepted);
}

TEST(Host, RunTickRunsJobsInStageOrderRegardlessOfRegistrationOrder) {
    Host host{make_sequence(), true};
    std::vector<std::string_view> executed;

    ASSERT_TRUE(
        host.schedule(stage::StageId{"Presentation"}, [&] { executed.emplace_back("Presentation"); }));
    ASSERT_TRUE(host.schedule(stage::StageId{"Input"}, [&] { executed.emplace_back("Input"); }));
    ASSERT_TRUE(host.schedule(stage::StageId{"Simulation"}, [&] { executed.emplace_back("Simulation"); }));

    host.run_tick();

    EXPECT_EQ(executed, (std::vector<std::string_view>{"Input", "Simulation", "Presentation"}));
}

TEST(Host, RunTickIsRepeatableAndBitExactAcrossRepeatedTicks) {
    Host host{make_sequence(), true};
    std::vector<std::string_view> executed;

    ASSERT_TRUE(host.schedule(stage::StageId{"Input"}, [&] { executed.emplace_back("Input"); }));

    host.run_tick();
    const std::vector<std::string_view> first_tick = executed;
    executed.clear();

    host.run_tick();
    const std::vector<std::string_view> second_tick = executed;

    EXPECT_EQ(first_tick, (std::vector<std::string_view>{"Input"}));
    EXPECT_EQ(first_tick, second_tick);
}

TEST(Host, SequenceAccessorReturnsTheConstructedSequence) {
    const Host host{make_sequence(), true};

    EXPECT_EQ(host.sequence().size(), 3U);
    EXPECT_TRUE(host.sequence().contains(stage::StageId{"Simulation"}));
    EXPECT_FALSE(host.sequence().contains(stage::StageId{"NotAStage"}));
}

// Entity lifecycle and tick execution are independent coordination surfaces
// composed by the same Host — creating entities must never disturb scheduled
// jobs or vice versa, since a real host interleaves both every tick.
TEST(Host, EntityLifecycleAndSchedulingDoNotInterfere) {
    Host host{make_sequence(), true};
    std::vector<int> executed;
    ASSERT_TRUE(host.schedule(stage::StageId{"Simulation"}, [&] { executed.push_back(1); }));

    const EntityRef first = host.create_entity();
    host.run_tick();
    const EntityRef second = host.create_entity();

    EXPECT_TRUE(host.is_entity_alive(first));
    EXPECT_TRUE(host.is_entity_alive(second));
    EXPECT_NE(first, second);
    EXPECT_EQ(executed, (std::vector<int>{1}));
}

} // namespace
} // namespace atlas::runtime
