#include "atlas/scheduler/scheduler.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <set>
#include <thread>
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

// --- run_tick_parallel() / schedule_parallel() -----------------------------
//
// These jobs deliberately sleep for a small, randomized duration *before*
// returning their Apply closure, to simulate real, unpredictable thread
// scheduling/timing jitter. The assertion in every test below is that the
// final, applied result never depends on which job's thread happened to
// finish first - only on registration order - matching the bit-exact
// determinism guarantee spec §4 requires even in the presence of real
// parallel execution.
std::chrono::microseconds jitter(unsigned int seed) {
    std::mt19937 engine{seed};
    std::uniform_int_distribution<int> distribution{0, 2000};
    return std::chrono::microseconds{distribution(engine)};
}

TEST(Scheduler, RunTickParallelWithNoRegisteredJobsDoesNothing) {
    const Scheduler scheduler{make_sequence()};

    scheduler.run_tick_parallel();
}

TEST(Scheduler, ScheduleParallelRejectsJobForStageNotInSequence) {
    Scheduler scheduler{make_sequence()};

    const bool accepted = scheduler.schedule_parallel(stage::StageId{"NotAStage"}, [] { return Apply{}; });

    EXPECT_FALSE(accepted);
}

TEST(Scheduler, ScheduleParallelAcceptsJobForStageInSequence) {
    Scheduler scheduler{make_sequence()};

    const bool accepted = scheduler.schedule_parallel(stage::StageId{"Simulation"}, [] { return Apply{}; });

    EXPECT_TRUE(accepted);
}

TEST(Scheduler, RunTickParallelSkipsEmptyApplyWithoutThrowing) {
    Scheduler scheduler{make_sequence()};
    ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Simulation"}, [] { return Apply{}; }));

    EXPECT_NO_THROW(scheduler.run_tick_parallel());
}

TEST(Scheduler, RunTickParallelAppliesResultsInRegistrationOrderDespiteThreadJitter) {
    for (unsigned int run = 0; run < 50; ++run) {
        Scheduler scheduler{make_sequence()};
        std::vector<int> executed;

        for (int index = 0; index < 8; ++index) {
            const unsigned int seed = (run * 8U) + static_cast<unsigned int>(index);
            ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Simulation"}, [index, seed, &executed] {
                std::this_thread::sleep_for(jitter(seed));
                // The concurrent phase does no shared-state writes at all;
                // it only returns the Apply closure that performs the
                // write-back once every job in the stage has finished.
                return Apply{[index, &executed] { executed.push_back(index); }};
            }));
        }

        scheduler.run_tick_parallel();

        EXPECT_EQ(executed, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}));
    }
}

TEST(Scheduler, RunTickParallelPreservesStageSequenceOrderAcrossStages) {
    Scheduler scheduler{make_sequence()};
    std::vector<std::string_view> executed;

    ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Presentation"}, [&] {
        std::this_thread::sleep_for(jitter(1));
        return Apply{[&] { executed.emplace_back("Presentation"); }};
    }));
    ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Input"}, [&] {
        std::this_thread::sleep_for(jitter(2));
        return Apply{[&] { executed.emplace_back("Input"); }};
    }));
    ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Simulation"}, [&] {
        std::this_thread::sleep_for(jitter(3));
        return Apply{[&] { executed.emplace_back("Simulation"); }};
    }));

    scheduler.run_tick_parallel();

    EXPECT_EQ(executed, (std::vector<std::string_view>{"Input", "Simulation", "Presentation"}));
}

TEST(Scheduler, RunTickParallelIsBitExactAcrossManyRepeatedRuns) {
    Scheduler scheduler{make_sequence()};
    for (int index = 0; index < 6; ++index) {
        ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Simulation"}, [index] {
            std::this_thread::sleep_for(jitter(static_cast<unsigned int>(index) * 97U + 13U));
            return Apply{[] {}};
        }));
    }

    std::vector<int> reference;
    reference.reserve(6);
    for (int index = 0; index < 6; ++index) {
        reference.push_back(index);
    }

    for (int run = 0; run < 200; ++run) {
        std::vector<int> executed;
        // Re-register per run so each run's Apply closures write into this
        // run's own `executed` vector, then re-run against a *fresh*
        // per-run scheduler built the same way - proving repeatability
        // rather than relying on state left over from a previous run.
        Scheduler per_run_scheduler{make_sequence()};
        for (int index = 0; index < 6; ++index) {
            const unsigned int seed = static_cast<unsigned int>(run) * 31U + static_cast<unsigned int>(index);
            ASSERT_TRUE(
                per_run_scheduler.schedule_parallel(stage::StageId{"Simulation"}, [index, seed, &executed] {
                    std::this_thread::sleep_for(jitter(seed));
                    return Apply{[index, &executed] { executed.push_back(index); }};
                }));
        }

        per_run_scheduler.run_tick_parallel();

        EXPECT_EQ(executed, reference);
    }
}

TEST(Scheduler, RunTickParallelDoesNotConsumeRegisteredJobs) {
    Scheduler scheduler{make_sequence()};
    std::vector<std::string_view> executed;

    ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Input"},
                                            [&] { return Apply{[&] { executed.emplace_back("Input"); }}; }));
    ASSERT_TRUE(scheduler.schedule_parallel(
        stage::StageId{"Simulation"}, [&] { return Apply{[&] { executed.emplace_back("Simulation"); }}; }));

    scheduler.run_tick_parallel();
    const std::vector<std::string_view> first_tick = executed;
    executed.clear();

    scheduler.run_tick_parallel();
    const std::vector<std::string_view> second_tick = executed;

    EXPECT_EQ(first_tick, (std::vector<std::string_view>{"Input", "Simulation"}));
    EXPECT_EQ(first_tick, second_tick);
}

TEST(Scheduler, RunTickParallelActuallyUsesMultipleThreads) {
    Scheduler scheduler{make_sequence()};
    std::mutex mutex;
    std::set<std::thread::id> thread_ids;

    for (int index = 0; index < 4; ++index) {
        ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Simulation"}, [&] {
            {
                const std::lock_guard<std::mutex> lock{mutex};
                thread_ids.insert(std::this_thread::get_id());
            }
            // Hold the thread open briefly so all four jobs are likely to
            // be in flight concurrently rather than finishing sequentially
            // fast enough to (spuriously) reuse one thread.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return Apply{[] {}};
        }));
    }

    scheduler.run_tick_parallel();

    EXPECT_GT(thread_ids.size(), 1U);
}

TEST(Scheduler, ScheduleAndScheduleParallelAreIndependentJobLists) {
    Scheduler scheduler{make_sequence()};
    std::vector<std::string_view> sequential_executed;
    std::vector<std::string_view> parallel_executed;

    ASSERT_TRUE(scheduler.schedule(stage::StageId{"Simulation"},
                                   [&] { sequential_executed.emplace_back("sequential"); }));
    ASSERT_TRUE(scheduler.schedule_parallel(stage::StageId{"Simulation"}, [&] {
        return Apply{[&] { parallel_executed.emplace_back("parallel"); }};
    }));

    scheduler.run_tick();

    EXPECT_EQ(sequential_executed, (std::vector<std::string_view>{"sequential"}));
    EXPECT_TRUE(parallel_executed.empty());

    scheduler.run_tick_parallel();

    EXPECT_EQ(parallel_executed, (std::vector<std::string_view>{"parallel"}));
}

} // namespace
} // namespace atlas::scheduler
