#pragma once

// Generated at build time from
// demo/modules/cast_time_attack/cast_time_attack.capability.yaml (see
// demo/CMakeLists.txt) - the CastTimeAttack/BeginCast/AdvanceCast/CastLanded
// contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/action.hpp"
#include "atlas/runtime/context.hpp"

#include <unordered_map>

#include "attack_resolution/attack_resolution.hpp"
#include "cast_time_attack.capability.hpp"
#include "haste/haste.hpp"
#include "interruption/interruption.hpp"
#include "movement/movement.hpp"

namespace atlas::cast_time_attack {

// The lifecycle piece of an in-progress cast (atlas::runtime::ActionState,
// atlas::runtime::Cancellable<T> - see atlas-runtime's own README section),
// kept separate from the generated CastTimeAttack property: atlas-cgen's
// manifest type system has no enum field type yet, so action_state can't
// live on the generated struct directly. This is this capability's own
// private per-entity bookkeeping alongside CastTimeAttack - the same shape
// armor::ContributionRegistry/movement::ContributionRegistry already
// establish for capability-owned state that isn't itself a manifest
// property.
struct CastAction {
    runtime::ActionState action_state = runtime::ActionState::Completed;
    bool cancel_requested = false;
};

static_assert(runtime::Cancellable<CastAction>);

// Owned by whoever composes this capability into a host (one per host,
// alongside its PropertyStore<CastTimeAttack>) - never a namespace-scope
// global, for the same reason armor::ContributionRegistry isn't one. A
// caster with no entry yet default-constructs to CastAction{} on first
// lookup (Completed, not cancel-requested) - "never cast anything" and "a
// previous cast already resolved" are the same idle bucket, deliberately,
// the same way remaining_ticks == 0 covered both before this refactor.
using ActionRegistry = std::unordered_map<EntityRef, CastAction>;

// The manual implementation of BeginCast's request handler (spec §14) -
// starts a wind-up: sets requires_stationary/target/obstacle/min_range/
// max_range/damage/animation from the request onto caster's CastTimeAttack
// property, and (in registry) transitions caster's CastAction to Started
// with cancel_requested cleared. Deliberately checks nothing about range or
// line of sight here - that validation happens once, at AdvanceCast's
// completion (see below), not at the start; a caster may begin casting at a
// target that's currently out of range, hoping to close the distance before
// the cast finishes.
//
// cast_time_ticks is resolved once, here, against caster's current
// effective haste::CastSpeed (ctx.get<haste::CastSpeed>(cmd.caster),
// defaulting to 1.0 - exactly like an entity with no Armor resolves to no
// mitigation - when the caster has none) and locked into both
// cast_time_ticks and remaining_ticks as the *effective* duration.
// CastSpeed belongs to haste, not this capability - this capability only
// reads it, the same way aura writes movement::MovementSpeed without
// movement knowing aura exists. It is never re-resolved mid-cast: a haste
// buff activated or refreshed after BeginCast has no effect on a cast
// already in progress, only on casts begun after it's active - deliberately,
// to avoid a cast's remaining duration jittering if the haste source's own
// range check flips mid-cast (see demo/README.md's discussion of this
// tradeoff). The resolved duration is published in CastStarted alongside
// caster's chosen animation, so a client can size its own animation
// playback to a cast that will actually complete in that many ticks -
// Atlas itself never touches animation beyond handing over this resource
// identity and duration (spec §3, Resource; spec §6, replicated state).
//
// Rejects if caster has no CastTimeAttack property, or if caster is already
// casting (registry's CastAction is Started or Ongoing from a previous,
// not-yet-complete BeginCast) - there is no instant-cancel-and-restart in
// this capability (see this capability's README section for the scope cut).
[[nodiscard]] RequestResult on_begin_cast(Context& ctx, ActionRegistry& registry, const BeginCast& cmd);

// The manual implementation of AdvanceCast's request handler (spec §14) -
// driven explicitly each call (delta_ticks simulation ticks elapsed since
// the last call), the same "caller simulates the tick" pattern
// auto_attack::on_try_auto_attack/aura::on_refresh_aura_effect already
// establish.
//
// Delegates the lifecycle step itself to atlas::runtime::advance_action,
// which checks caster's CastAction::cancel_requested *before* running any
// of this function's own per-tick logic (see atlas-runtime's own README
// section): if a cancellation is pending (queued by on_movement_occurred or
// on_action_interrupted below - queued, not applied immediately, so this is
// the one well-defined point every cast actually gets cancelled at),
// remaining_ticks resets to 0 and neither attack_resolution nor CastLanded
// are ever reached this call. An already-terminal CastAction (Cancelled or
// Completed - never cast, or a previous cast already resolved) is an
// ordinary no-op, the same accept-as-no-op precedent auto_attack/aura/
// pathing already establish for their own "nothing to do yet" cases.
//
// Otherwise: ticks remaining_ticks down by delta_ticks (saturating at 0).
// Not yet 0 is the ordinary "still casting" outcome (CastAction moves to
// Ongoing). Once it reaches 0, CastAction moves to Completed and the cast
// resolves: delegates to attack_resolution::resolve_targeted_attack (shared
// with auto_attack, not reimplemented here) using the target/obstacle/
// min_range/max_range/damage BeginCast locked in. Its result is propagated
// unchanged - a reject (caster/target has no movement::Position, or target
// has no Health once an attempt is actually made) surfaces as-is; an
// accepted-but-not-landed outcome (target moved out of range, or line of
// sight is now blocked) is a fizzle - there is no lingering "waiting for
// range" state, and CastLanded is published only when
// resolve_targeted_attack reports landed = true.
//
// Rejects if caster has no CastTimeAttack property.
[[nodiscard]] RequestResult on_advance_cast(Context& ctx, ActionRegistry& registry, const AdvanceCast& cmd);

// Cancellation, part one: movement, opt-in per cast. Not a request handler -
// a subscriber meant to be registered against movement::PositionChanged by
// whoever composes this capability into a host (see
// demo/tests/simulated_host.hpp), the same way a capability's own request
// handlers are registered against a request::Dispatcher rather than called
// directly.
//
// Queues a cancellation (atlas::runtime::request_cancel) - it does not
// cancel outright. The actual transition to Cancelled only happens the
// next time on_advance_cast runs (via advance_action), matching "the
// runtime handles cancel first" as literal control flow rather than an
// out-of-band mutation the instant this event arrives.
//
// Only queues one when event.entity has a registry entry (i.e. has cast
// something before - an entity that never has is simply irrelevant, not an
// error) and that specific cast opted in (CastTimeAttack::requires_stationary,
// set at BeginCast time - "some attacks require the caster to stand still,"
// not every attack). An entity with no CastTimeAttack property, or a cast
// that didn't opt into requires_stationary, is left untouched.
void on_movement_occurred(Context& ctx, ActionRegistry& registry, const movement::PositionChanged& event);

// Cancellation, part two: interruption::ActionInterrupted, unconditional.
// The generic mechanism a crowd-control effect (a stun, a disorient - not
// yet built in this demo, see this capability's README section) would
// trigger: unlike on_movement_occurred above, this ignores
// requires_stationary entirely - being stunned interrupts a cast regardless
// of whether that specific cast cared about the caster's own movement.
// Queues a cancellation exactly like on_movement_occurred does whenever
// event.entity has a registry entry; otherwise a no-op. Needs no Context at
// all - registry alone is enough to know whether this entity is this
// capability's concern.
void on_action_interrupted(ActionRegistry& registry, const interruption::ActionInterrupted& event);

} // namespace atlas::cast_time_attack
