#include "atlas/scheduler/scheduler.hpp"

#include <future>

namespace atlas::scheduler {

Scheduler::Scheduler(stage::StageSequence sequence) : sequence_(std::move(sequence)) {
    jobs_by_stage_.reserve(sequence_.size());
    parallel_jobs_by_stage_.reserve(sequence_.size());
    for (const auto& stage_id : sequence_) {
        jobs_by_stage_.emplace_back(stage_id, std::vector<Job>{});
        parallel_jobs_by_stage_.emplace_back(stage_id, std::vector<ParallelJob>{});
    }
}

bool Scheduler::schedule(const stage::StageId& stage_id, Job job) {
    for (auto& [id, jobs] : jobs_by_stage_) {
        if (id == stage_id) {
            jobs.push_back(std::move(job));
            return true;
        }
    }
    return false;
}

void Scheduler::run_tick() const {
    for (const auto& stage_jobs : jobs_by_stage_) {
        for (const auto& job : stage_jobs.second) {
            job();
        }
    }
}

bool Scheduler::schedule_parallel(const stage::StageId& stage_id, ParallelJob job) {
    for (auto& [id, jobs] : parallel_jobs_by_stage_) {
        if (id == stage_id) {
            jobs.push_back(std::move(job));
            return true;
        }
    }
    return false;
}

void Scheduler::run_tick_parallel() const {
    for (const auto& stage_jobs : parallel_jobs_by_stage_) {
        const auto& jobs = stage_jobs.second;
        if (jobs.empty()) {
            continue;
        }

        // Concurrent phase: launch every job registered against this stage
        // on its own thread. std::launch::async forces an actual new
        // thread per job rather than leaving the policy up to the
        // implementation (which could otherwise silently run every job
        // deferred/lazily on the calling thread, defeating the point).
        std::vector<std::future<Apply>> futures;
        futures.reserve(jobs.size());
        for (const auto& job : jobs) {
            futures.push_back(std::async(std::launch::async, job));
        }

        // Barrier: collect every job's Apply closure before applying any of
        // them. future::get() blocks until that specific job is done, but
        // every job was already launched above, so this loop only
        // serializes when we start reading results, not the jobs'
        // execution itself.
        std::vector<Apply> applies;
        applies.reserve(futures.size());
        for (auto& future : futures) {
            applies.push_back(future.get());
        }

        // Merge phase: apply every result strictly in registration order,
        // regardless of which underlying thread actually finished first —
        // this is what keeps the stage's effect on shared state bit-exact
        // and reproducible (spec §4).
        for (const auto& apply : applies) {
            if (apply) {
                apply();
            }
        }
    }
}

const stage::StageSequence& Scheduler::sequence() const noexcept {
    return sequence_;
}

} // namespace atlas::scheduler
