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

// Runs jobs registered against an atlas::stage::StageSequence, stage by
// stage in the sequence's fixed order, and within a stage in the exact order
// jobs were registered (spec §4, Deterministic Execution: "a fixed,
// reproducible stage and job order is part of the determinism guarantee, not
// merely an optimization"). Single-threaded and strictly sequential by
// design — concurrent/parallel job execution is out of scope here (spec §4
// calls unordered iteration over concurrent work an architectural defect,
// not a day-one optimization target).
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

    [[nodiscard]] const stage::StageSequence& sequence() const noexcept;

private:
    stage::StageSequence sequence_;
    // One entry per stage in sequence_'s order, populated once at
    // construction time; schedule() only ever appends within an existing
    // entry's job list, never reorders or adds entries. A vector keyed by
    // linear scan (rather than a hash-based map) keeps lookup itself free of
    // any non-deterministic iteration order, consistent with §4.
    std::vector<std::pair<stage::StageId, std::vector<Job>>> jobs_by_stage_;
};

} // namespace atlas::scheduler
