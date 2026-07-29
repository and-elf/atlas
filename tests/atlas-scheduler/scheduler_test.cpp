#include "atlas/scheduler/scheduler.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace atlas::scheduler {
namespace {

stage::StageSequence make_sequence() {
    auto sequence = stage::StageSequence::create({stage::StageId{"Input"},
                                                  stage::StageId{"Simulation"},
                                                  stage::StageId{"Replication"},
                                                  stage::StageId{"Presentation"}});
    return std::move(*sequence);
}

TEST(Scheduler, RejectsJobForStageNotInSequence) {
    Scheduler scheduler{make_sequence()};

    const bool accepted = scheduler.schedule(stage::StageId{"NotAStage"}, [] {});

    EXPECT_FALSE(accepted);
}

TEST(Scheduler, AcceptsJobForStageInSequence) {
    Scheduler scheduler{make_sequence()};

    const bool accepted = scheduler.schedule(stage::StageId{"Simulation"}, [] {});

    EXPECT_TRUE(accepted);
}

TEST(Scheduler, RunTickWithNoRegisteredJobsDoesNothing) {
    const Scheduler scheduler{make_sequence()};

    scheduler.run_tick();
}

TEST(Scheduler, RunsJobsInStageSequenceOrderRegardlessOfRegistrationOrder) {
    Scheduler scheduler{make_sequence()};
    std::vector<std::string_view> executed;

    // Registered out of stage order on purpose: execution order must follow
    // the sequence's fixed order, not the order schedule() happened to be
    // called in.
    ASSERT_TRUE(
        scheduler.schedule(stage::StageId{"Presentation"}, [&] { executed.emplace_back("Presentation"); }));
    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Input"}, [&] { executed.emplace_back("Input"); }));
    ASSERT_TRUE(
        scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.emplace_back("Simulation"); }));

    scheduler.run_tick();

    EXPECT_EQ(executed, (std::vector<std::string_view>{"Input", "Simulation", "Presentation"}));
}

TEST(Scheduler, RunsJobsWithinAStageInRegistrationOrder) {
    Scheduler scheduler{make_sequence()};
    std::vector<int> executed;

    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.push_back(1); }));
    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.push_back(2); }));
    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.push_back(3); }));

    scheduler.run_tick();

    EXPECT_EQ(executed, (std::vector<int>{1, 2, 3}));
}

TEST(Scheduler, RejectedScheduleDoesNotRunAndDoesNotDisturbValidJobs) {
    Scheduler scheduler{make_sequence()};
    std::vector<int> executed;

    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.push_back(1); }));
    ASSERT_FALSE(scheduler.schedule(stage::StageId{"NotAStage"}, [&] { executed.push_back(99); }));
    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.push_back(2); }));

    scheduler.run_tick();

    EXPECT_EQ(executed, (std::vector<int>{1, 2}));
}

TEST(Scheduler, RunTickIsRepeatableAndBitExactAcrossRepeatedTicks) {
    Scheduler scheduler{make_sequence()};
    std::vector<std::string_view> executed;

    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Input"}, [&] { executed.emplace_back("Input"); }));
    ASSERT_TRUE(
        scheduler.schedule(stage::StageId{"Simulation"}, [&] { executed.emplace_back("Simulation"); }));

    scheduler.run_tick();
    const std::vector<std::string_view> first_tick = executed;
    executed.clear();

    scheduler.run_tick();
    const std::vector<std::string_view> second_tick = executed;

    EXPECT_EQ(first_tick, (std::vector<std::string_view>{"Input", "Simulation"}));
    EXPECT_EQ(first_tick, second_tick);
}

TEST(Scheduler, EmptySequenceRejectsAnySchedule) {
    auto empty_sequence = stage::StageSequence::create({});
    ASSERT_TRUE(empty_sequence.has_value());
    Scheduler scheduler{std::move(*empty_sequence)};

    EXPECT_FALSE(scheduler.schedule(stage::StageId{"Simulation"}, [] {}));
}

TEST(Scheduler, SequenceAccessorReturnsTheConstructedSequence) {
    const Scheduler scheduler{make_sequence()};

    EXPECT_EQ(scheduler.sequence().size(), 4U);
    EXPECT_TRUE(scheduler.sequence().contains(stage::StageId{"Simulation"}));
    EXPECT_FALSE(scheduler.sequence().contains(stage::StageId{"NotAStage"}));
}

} // namespace
} // namespace atlas::scheduler
