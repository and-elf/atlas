#pragma once

// Generated at build time from
// demo/modules/rigid_body/rigid_body.capability.yaml (see demo/CMakeLists.txt)
// - the BodyState/SpawnRigidBody contracts.
//
// Issue #188: proves atlas::physics::PhysicsBackend (libraries/atlas-physics)
// feeding a real, depends_on/consumes-ordered capability dependency graph -
// spec §5's own illustrative `Physics["physics"] --> Entity` diagram
// (docs/specification/05-dependency-model.md), made real. Named rigid_body,
// deliberately not physics: atlas::physics is already the real runtime
// library's own C++ namespace - this capability is a demo/-level *consumer*
// of it (spec §2, Mechanism Over Meaning: atlas-physics is the mechanism,
// this is one gameplay-facing use of it), and reusing the same name would
// collide with that namespace.
//
// The "runs every tick regardless" mechanism (schedule_step_job, below) is
// NOT the "caller dispatches an Advance*-shaped request with its own
// delta_ticks field" pattern every other per-tick-shaped demo capability
// uses (movement/aura/pathing/auto_attack/cast_time_attack/damage_over_time -
// see demo/README.md, "No tick scheduler driving RefreshAuraEffect
// automatically... this demo doesn't build that job itself"). That pattern
// doesn't fit here: PhysicsBackend::step() must run exactly once per tick
// regardless of how many bodies or requests exist that tick, not once per
// dispatched request. Investigating atlas-stage/atlas-scheduler/atlas-runtime
// found the real mechanism already exists, just never used by a demo
// capability yet: atlas::runtime::Host::schedule(StageId, Job) registers a
// std::function<void()> against a stage in the host's own
// atlas::stage::StageSequence, and atlas::runtime::Host::run_tick() (invoked
// automatically, every tick, via atlas::advance_tick - demo/host_loop.cpp's
// own run_ticks, "the first place in this codebase that actually exercises
// Host::run_tick()") runs every registered job exactly once, in fixed,
// deterministic stage-then-registration order (atlas::scheduler::Scheduler,
// spec §4). demo/host_loop.cpp's own doc comment even flags this precise gap
// ("no gameplay logic is scheduled against any stage here") - this
// capability is the first demo consumer of that existing mechanism, not a
// new one invented for this issue.
#include "atlas/core/time.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/physics/body.hpp"
#include "atlas/physics/body_id.hpp"
#include "atlas/physics/null_physics_backend.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/stage/stage_id.hpp"

#if defined(ATLAS_DEMO_PHYSICS_BACKEND_JOLT)
#    include "atlas/physics/jolt_physics_backend.hpp"
#endif

#include <utility>
#include <vector>

#include "movement/movement.hpp"
#include "rigid_body.capability.hpp"

namespace atlas::rigid_body {

// Which concrete atlas::physics::PhysicsBackend this capability instantiates
// - resolved at compile time, never a runtime factory/plugin lookup (spec
// §4), from the same ATLAS_PHYSICS_BACKEND CMake option atlas-physics' own
// CMakeLists.txt already exposes (never a second, independent option):
// demo/modules/rigid_body/CMakeLists.txt defines
// ATLAS_DEMO_PHYSICS_BACKEND_JOLT only when ATLAS_PHYSICS_BACKEND=JOLT,
// generalizing tests/atlas-physics/CMakeLists.txt's own
// ATLAS_PHYSICS_BACKEND-conditional-sources pattern from "which .cpp gets
// compiled" to "which type gets instantiated", since this capability (unlike
// a test binary that simply omits the Jolt-only test file under the NULL
// backend) needs exactly one concrete backend type selected inside a single
// translation unit.
#if defined(ATLAS_DEMO_PHYSICS_BACKEND_JOLT)
using Backend = physics::JoltPhysicsBackend;
#else
using Backend = physics::NullPhysicsBackend;
#endif

// The fixed per-tick timestep advance_rigid_bodies/schedule_step_job step
// the backend by - derived from atlas::core::Time::ticks_per_second (spec
// §4's own deterministic tick rate), never a caller-guessed constant.
inline constexpr float k_fixed_timestep_seconds = 1.0F / static_cast<float>(core::Time::ticks_per_second);

// This capability's own private per-entity bookkeeping (mirrors
// movement::ContributionRegistry/auto_attack::ActionRegistry): the EntityRef
// a SpawnRigidBody request named -> the BodyId the backend actually handed
// back. Owned by whoever composes this capability into a host, never a
// namespace-scope global, for the same reason those registries aren't one.
//
// A plain, insertion-ordered std::vector of pairs - deliberately not a
// std::unordered_map. advance_rigid_bodies (below) is this demo's first
// per-tick job that iterates *every* tracked entity in one call with no
// request batch driving that iteration (every other per-tick-shaped
// function in this demo is instead called once per entity by an explicit
// request - see this file's own top-of-file comment). atlas-runtime's own
// README names exactly this shape of node as needing a canonical order and
// names entity-creation order as "the natural candidate, since entity
// creation is itself already a deterministic sequence" - a
// std::unordered_map's iteration order would violate spec §4's "avoid
// unordered iteration over ... work anywhere it could affect simulation
// state" for no benefit this registry's own expected size needs. Mirrors
// atlas::scheduler::Scheduler's own jobs_by_stage_ linear-scan-over-a-vector
// choice for the identical reason.
using BodyRegistry = std::vector<std::pair<EntityRef, physics::BodyId>>;

// The manual implementation of SpawnRigidBody's request handler (spec §14).
// Creates a real backend body for cmd.entity, seeded at cmd.entity's current
// movement::Position - the "HasPosition-shaped contract this capability
// depends on for initial spawn placement" investigated for this issue rather
// than inventing a second source of initial position data (demo/README.md's
// own "Physics" section covers this investigation) - mapped onto the
// physics world's horizontal (X, Z) ground plane, with cmd.spawn_height as
// the vertical (Y) coordinate Jolt's own gravity acts along:
// movement::Position is a flat 2D (x, y) ground-plane coordinate
// (movement.capability.yaml) with no vertical component of its own to reuse,
// so cmd.spawn_height is this request's own explicit field for it.
//
// Rejects if entity already has a tracked body (no duplicate spawn,
// mirroring door::on_open_door's own "already open" rejection shape), or if
// entity has no movement::Position seeded (a setup mistake, matching
// movement::on_move's own precedent for a missing Position - an entity being
// given a rigid body is expected to already have a ground-plane position to
// seed it from).
//
// The created body is queried once immediately and written into ctx's
// BodyState property, so a caller reading it before this tick's scheduled
// step job (schedule_step_job, below) next runs still observes a real,
// backend-reported pose rather than a default-constructed one.
[[nodiscard]] RequestResult
on_spawn_rigid_body(Context& ctx, Backend& backend, BodyRegistry& registry, const SpawnRigidBody& cmd);

// The actual per-tick physics step (issue #188's own core mechanism): steps
// backend forward by exactly delta_seconds - a caller-supplied fixed
// timestep, never sourced from a clock (spec §4; physics_backend.hpp's own
// step() doc comment) - exactly once, regardless of how many bodies
// registry tracks, then reads every tracked body's current BodyState back
// from the backend and writes it into ctx's BodyState property store. A
// tracked entity whose backend body no longer exists (e.g. destroyed
// directly through the backend) is silently left with whatever BodyState it
// last had, rather than erased or overwritten with a bogus value.
//
// A free function, not folded directly into schedule_step_job below, so it
// stays directly unit-testable without a Host/Scheduler in the way.
void advance_rigid_bodies(Context& ctx, Backend& backend, const BodyRegistry& registry, float delta_seconds);

// Registers advance_rigid_bodies as a Job against stage_id in host's own
// StageSequence (atlas::scheduler::Scheduler::schedule, via
// atlas::runtime::Host::schedule) - see this file's own top-of-file comment
// for the full investigation of why this is the real "runs every tick
// regardless" mechanism, not an invented one. host.run_tick() (called
// automatically every tick via atlas::advance_tick) then invokes this job
// exactly once per tick, in the same fixed, deterministic stage/job order
// every other job registered against this stage runs in (spec §4).
//
// Registering this job *before* physics_observer::schedule_observation_job
// against the same stage is what makes that capability's own read of
// BodyState observe this tick's freshly-stepped value, never the previous
// tick's - see demo/tests/rigid_body_test.cpp for the test that proves this
// ordering is load-bearing, not merely assumed from call order.
//
// Returns false, and leaves host unchanged, if stage_id is not part of
// host's own StageSequence - mirrors Host::schedule's/Scheduler::schedule's
// own rejection contract.
[[nodiscard]] bool schedule_step_job(runtime::Host& host,
                                     const stage::StageId& stage_id,
                                     Context& ctx,
                                     Backend& backend,
                                     const BodyRegistry& registry,
                                     float delta_seconds);

} // namespace atlas::rigid_body
