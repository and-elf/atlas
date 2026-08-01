#pragma once

#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <functional>
#include <utility>
#include <vector>

namespace atlas::scheduler {

// A registered unit of work. Deliberately just a callable with no context
// parameter: the typed, monadic context API request/event handlers use
// (§21) belongs to the not-yet-built capability/runtime layer, not to this
// library's scope — atlas-scheduler only walks a fixed order and invokes
// whatever was registered against each step.
using Job = std::function<void()>;

// A unit of work registered for run_tick_parallel(), split into two phases
// so the concurrency boundary and the determinism boundary are two
// separately-reasoned-about things:
//
//   1. The ParallelJob itself runs concurrently, on its own thread, alongside
//      every other ParallelJob registered against the same stage. Per §4
//      (Deterministic Execution — "unordered iteration over concurrent or
//      parallel work" is named as an architectural defect), this phase must
//      not touch state shared with any other job registered against that
//      stage: a ParallelJob is only safe to register when it is provably
//      independent of every other job in the same stage. It returns an Apply
//      closure that performs whatever write-back into shared/simulation
//      state its computation implies, rather than performing that write-back
//      itself.
//   2. Every Apply closure for a stage runs only after every ParallelJob in
//      that stage has finished its concurrent phase, and strictly in
//      registration order — never in whatever order the underlying threads
//      happened to finish. This is the merge step §4 requires: parallelism
//      is permitted for the computation, never for the order results land in
//      shared state. An empty Apply (default-constructed) is treated as "no
//      write-back" and simply skipped, rather than invoked.
using Apply = std::function<void()>;
using ParallelJob = std::function<Apply()>;

// Runs jobs registered against an atlas::stage::StageSequence, stage by
// stage in the sequence's fixed order, and within a stage in the exact order
// jobs were registered (spec §4, Deterministic Execution: "a fixed,
// reproducible stage and job order is part of the determinism guarantee, not
// merely an optimization"). run_tick() itself is single-threaded and
// strictly sequential by design. run_tick_parallel() is the opt-in
// counterpart that runs a stage's jobs concurrently across threads while
// still merging their results back in a fixed, deterministic order — see
// ParallelJob above and run_tick_parallel()'s own comment.
//
// An encapsulated class rather than a plain aggregate: it protects the
// invariant that a job can only be registered against a stage that is
// actually part of the sequence it was constructed with — the same
// "explicitly reject invalid input" pattern as
// atlas::stage::StageSequence::create (duplicate stage) and
// atlas::entity::EntityRegistry::destroy (stale/unknown ref).
class Scheduler {
public:
    explicit Scheduler(stage::StageSequence sequence);

    // Appends job to the jobs already registered for stage_id, to run after
    // all of them on the next run_tick(). Returns false, and leaves the
    // scheduler unchanged, if stage_id is not part of the sequence this
    // scheduler was constructed with, rather than silently dropping the job
    // or throwing.
    bool schedule(const stage::StageId& stage_id, Job job);

    // Runs every registered job exactly once, stage by stage in the
    // sequence's order and, within each stage, in registration order.
    // Registered jobs are not consumed: calling run_tick() again re-runs the
    // same jobs in the same order, matching how a real host drives repeated
    // ticks against a persistently-composed set of work.
    void run_tick() const;

    // Appends job to the parallel jobs already registered for stage_id, to
    // run on the next run_tick_parallel(). Same rejection contract as
    // schedule(): returns false, and leaves the scheduler unchanged, if
    // stage_id is not part of the sequence this scheduler was constructed
    // with. Storage is independent from schedule()'s plain Job list — the
    // two mechanisms never interact within a stage, and each is only run by
    // its own run_tick()/run_tick_parallel() method.
    bool schedule_parallel(const stage::StageId& stage_id, ParallelJob job);

    // Opt-in parallel counterpart to run_tick(). Stages still execute one at
    // a time, in the sequence's fixed order (a later stage may depend on an
    // earlier one's output) — only the ParallelJobs *within* a stage run
    // concurrently, each on its own thread. Every job in a stage runs to
    // completion before that stage's Apply closures are invoked serially, in
    // registration order — so the final effect on shared state is identical
    // to running the same jobs sequentially, regardless of real-world
    // thread scheduling/timing. Registered ParallelJobs are not consumed,
    // mirroring run_tick().
    void run_tick_parallel() const;

    [[nodiscard]] const stage::StageSequence& sequence() const noexcept;

private:
    stage::StageSequence sequence_;
    // One entry per stage in sequence_'s order, populated once at
    // construction time; schedule() only ever appends within an existing
    // entry's job list, never reorders or adds entries. A vector keyed by
    // linear scan (rather than a hash-based map) keeps lookup itself free of
    // any non-deterministic iteration order, consistent with §4.
    std::vector<std::pair<stage::StageId, std::vector<Job>>> jobs_by_stage_;
    // Same shape and same linear-scan rationale as jobs_by_stage_, but for
    // ParallelJob/run_tick_parallel() — kept fully separate so run_tick()'s
    // existing sequential behavior is untouched by this addition.
    std::vector<std::pair<stage::StageId, std::vector<ParallelJob>>> parallel_jobs_by_stage_;
};

} // namespace atlas::scheduler
