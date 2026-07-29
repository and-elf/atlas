#pragma once

#include "atlas/entity/entity_registry.hpp"
#include "atlas/scheduler/scheduler.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

namespace atlas::runtime {

// The concrete runtime object a server or client process actually
// instantiates to hold state and drive execution (spec §13: "host execution
// environment, runtime integration, coordination between systems"). This is
// deliberately not the eventual capability-composed host described in §7/§8
// — that version is assembled by tooling from capability manifests, which
// don't exist yet (§14, Declarative Source Format). Host is the hand-composed
// substrate such a manifest-driven host would eventually sit on top of: it
// owns exactly the systems that already exist in this repo
// (atlas::entity::EntityRegistry, atlas::scheduler::Scheduler over an
// atlas::stage::StageSequence) and exposes their operations under one
// coordination point, matching how §8 frames every host — server, client,
// editor, test harness, CLI tool alike — as sharing the same runtime
// architecture rather than each getting a bespoke shape.
//
// Whether/how Host should model server-vs-client authority (§6: "authority
// is a responsibility of hosts") is deliberately not decided here — see this
// library's README for why that's left as an open question rather than
// guessed at.
//
// An encapsulated class, not a plain aggregate: EntityRegistry and Scheduler
// already each protect their own invariant privately, and Host's entire
// reason to exist is to be the single owner that composes them — there is no
// public field a caller could usefully poke directly without bypassing one
// of the delegating methods below.
class Host {
public:
    explicit Host(stage::StageSequence sequence);

    // Delegates to EntityRegistry::create — see its docs for the
    // generational-index lifecycle guarantee.
    [[nodiscard]] EntityRef create_entity();

    // Delegates to EntityRegistry::destroy — returns false for an unknown,
    // already-destroyed, or stale-generation reference rather than
    // asserting, matching EntityRegistry's own contract.
    bool destroy_entity(EntityRef ref) noexcept;

    [[nodiscard]] bool is_entity_alive(EntityRef ref) const noexcept;

    // Delegates to Scheduler::schedule — returns false, and leaves the host
    // unchanged, if stage_id is not part of the sequence this host was
    // constructed with.
    bool schedule(const stage::StageId& stage_id, scheduler::Job job);

    // Delegates to Scheduler::run_tick — runs every registered job exactly
    // once, stage by stage in the sequence's fixed order and, within a
    // stage, in registration order.
    void run_tick() const;

    [[nodiscard]] const stage::StageSequence& sequence() const noexcept;

private:
    entity::EntityRegistry entities_;
    scheduler::Scheduler scheduler_;
};

} // namespace atlas::runtime
