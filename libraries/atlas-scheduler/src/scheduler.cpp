#include "atlas/scheduler/scheduler.hpp"

namespace atlas::scheduler {

Scheduler::Scheduler(stage::StageSequence sequence) : sequence_(std::move(sequence)) {
    jobs_by_stage_.reserve(sequence_.size());
    for (const auto& stage_id : sequence_) {
        jobs_by_stage_.emplace_back(stage_id, std::vector<Job>{});
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

const stage::StageSequence& Scheduler::sequence() const noexcept {
    return sequence_;
}

} // namespace atlas::scheduler
