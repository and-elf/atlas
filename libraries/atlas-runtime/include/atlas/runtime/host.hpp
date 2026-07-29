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
// Authority (§6: "authority is a responsibility of hosts") is a plain bool
// flag, set once at construction and never changed afterward — resolving
// the open question this README previously left undecided. Matches §21's
// own worked example exactly (`ctx.host().has_authority()`): a request
// handler asks the host, not the capability, whether it may mutate
// authoritative state. A richer representation (distinct ServerHost/
// ClientHost types, or a capability-level concern layered on top) remains
// possible later; this is deliberately the smallest thing that answers the
// one question §21's handler code actually asks. See this library's README
// for the reasoning in full.
//
// An encapsulated class, not a plain aggregate: EntityRegistry and Scheduler
// already each protect their own invariant privately, and Host's entire
// reason to exist is to be the single owner that composes them — there is no
// public field a caller could usefully poke directly without bypassing one
// of the delegating methods below.
class Host {
public:
    explicit Host(stage::StageSequence sequence, bool has_authority);

    // §6, §21: whether this host may authoritatively mutate state through
    // an accepted request. A request handler checks this (never guesses at
    // authority from its own composition) before applying any mutation.
    [[nodiscard]] bool has_authority() const noexcept;

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
    bool has_authority_;
    entity::EntityRegistry entities_;
    scheduler::Scheduler scheduler_;
};

} // namespace atlas::runtime
