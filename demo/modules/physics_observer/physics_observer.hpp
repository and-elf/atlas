#pragma once

// Generated at build time from
// demo/modules/physics_observer/physics_observer.capability.yaml (see
// demo/CMakeLists.txt) - the ObservedBodyState contract.
//
// Issue #188's own "second, minimal downstream demo capability": consumes
// rigid_body's BodyState property (consumes: [BodyState] - spec §5,
// Property-Level Ordering - not a depends_on: [rigid_body] entry, the same
// consumes: idiom cast_time_attack's own CastSpeed read already establishes)
// so a real test can prove the dependency graph itself orders "physics has
// stepped for this tick" before "something reads the result" - see
// rigid_body.hpp / demo/tests/rigid_body_test.cpp for the full mechanism
// this proves.
//
// Deliberately minimal (spec §188's own "prove the mechanism, not gameplay
// semantics" scope): this capability has no request of its own at all - its
// only job is to read another capability's composed property and copy one
// field of it into its own, on a schedule (see schedule_observation_job,
// below), the same "capability with no requests/events of its own, purely a
// reader" shape line_of_sight's query-only role establishes, generalized
// from "callable directly" to "scheduled as a per-tick job".
#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/stage/stage_id.hpp"

#include <vector>

#include "physics_observer.capability.hpp"
#include "rigid_body/rigid_body.hpp"

namespace atlas::physics_observer {

// The fixed set of entities this capability watches each tick - a plain,
// caller-supplied list rather than a registry this capability builds up
// itself via some "start watching"-shaped request: this capability's only
// job is to prove the read side of the ordering, not to grow its own
// gameplay-shaped subscription mechanism (out of scope per issue #188's own
// "prove the mechanism, not gameplay semantics" boundary).
using WatchList = std::vector<EntityRef>;

// Reads rigid_body::BodyState for every entity in watched and copies its
// height (BodyState::position_y - the vertical, gravity-affected axis, see
// rigid_body.hpp) into this capability's own ObservedBodyState property.
// Entities in watched with no BodyState yet (never spawned, or spawned but
// not yet stepped this run) are silently skipped - the same "absent is an
// ordinary outcome, not a setup mistake" precedent every other
// consumes-shaped read in this demo already follows (e.g. health.cpp
// reading a possibly-absent armor::Armor).
void observe_body_heights(Context& ctx, const WatchList& watched);

// Registers observe_body_heights as a Job against stage_id in host's own
// StageSequence - the same mechanism rigid_body::schedule_step_job uses
// (see its own doc comment, rigid_body.hpp). Registering this job *after*
// rigid_body::schedule_step_job, against the same stage, is what makes this
// capability's own read of BodyState observe that tick's freshly-stepped
// value rather than the previous tick's - see
// demo/tests/rigid_body_test.cpp for the test that actually proves this
// ordering is load-bearing, rather than merely asserting it.
//
// Returns false, and leaves host unchanged, if stage_id is not part of
// host's own StageSequence - mirrors Host::schedule's/Scheduler::schedule's
// own rejection contract. Takes watched by value and moves it into the
// registered Job's own closure, rather than capturing a caller-owned
// container by reference: a Job persists and is invoked repeatedly for as
// long as the host runs (atlas::scheduler::Scheduler::run_tick's own
// contract - "registered jobs are not consumed"), so it must own whatever
// state it needs rather than risk a dangling reference to a caller's
// shorter-lived local.
[[nodiscard]] bool schedule_observation_job(runtime::Host& host,
                                            const stage::StageId& stage_id,
                                            Context& ctx,
                                            WatchList watched);

} // namespace atlas::physics_observer
