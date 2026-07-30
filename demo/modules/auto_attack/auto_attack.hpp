#pragma once

// Generated at build time from
// demo/modules/auto_attack/auto_attack.capability.yaml (see
// demo/CMakeLists.txt) - the WeaponAttack/QueueAttackBonus/TryAutoAttack/
// AutoAttackLanded contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/action.hpp"
#include "atlas/runtime/context.hpp"

#include <unordered_map>

#include "attack_resolution/attack_resolution.hpp"
#include "auto_attack.capability.hpp"
#include "interruption/interruption.hpp"
#include "movement/movement.hpp"

namespace atlas::auto_attack {

// The lifecycle piece of a weapon's swing cycle (atlas::runtime::ActionState,
// atlas::runtime::Cancellable<T> - see atlas-runtime's own README section),
// kept separate from the generated WeaponAttack property for the same
// reason cast_time_attack::CastAction is: atlas-cgen's manifest type system
// has no enum field type yet. Unlike CastAction, a fresh entry's default
// action_state is Started, not Completed - there is no separate "opt in"
// request the way BeginCast is for casts; on_try_auto_attack itself must
// always be able to proceed on an entity's very first call, so a fresh
// registry entry can never start in a terminal state (advance_action would
// otherwise skip it outright, and the very first swing would never
// happen).
struct WeaponAction {
    runtime::ActionState action_state = runtime::ActionState::Started;
    bool cancel_requested = false;
};

static_assert(runtime::Cancellable<WeaponAction>);

// Owned by whoever composes this capability into a host (one per host,
// alongside its PropertyStore<WeaponAttack>) - never a namespace-scope
// global, for the same reason armor::ContributionRegistry isn't one.
using ActionRegistry = std::unordered_map<EntityRef, WeaponAction>;

// The manual implementation of QueueAttackBonus's request handler (spec
// §14) - the "enhances a single auto-attack" shape (a Heroic Strike-style
// ability): accumulates onto attacker's WeaponAttack::pending_bonus_damage
// (+=, not overwrite - more than one queued bonus can stack before the
// next swing actually lands) and touches nothing else. No cooldown change,
// no damage dealt here, no target even named - this ability doesn't
// attack, it primes whichever swing on_try_auto_attack below lands next.
// Rejects if attacker has no WeaponAttack property.
[[nodiscard]] RequestResult on_queue_attack_bonus(Context& ctx, const QueueAttackBonus& cmd);

// The manual implementation of TryAutoAttack's request handler (spec §14) -
// the cyclic melee/ranged auto-attack. Driven explicitly each call
// (delta_ticks simulation ticks elapsed since the last call), the same
// "caller simulates the tick" pattern aura::on_refresh_aura_effect and
// pathing::on_advance_pathing already establish - this demo doesn't build
// the scheduler job that would call it automatically every host tick.
//
// Delegates the lifecycle step to atlas::runtime::advance_action, which
// checks attacker's WeaponAction::cancel_requested *before* running any of
// this function's own per-tick logic (see atlas-runtime's own README
// section): a pending cancellation (queued by on_movement_occurred or
// on_action_interrupted below) resets cooldown_remaining_ticks to
// attack_speed_ticks - the same full-cycle penalty a swing interrupted
// partway through pays - *unless* the weapon was already ready
// (cooldown_remaining_ticks == 0), in which case there is nothing
// in-progress to interrupt and cancellation is a deliberate no-op. Unlike
// cast_time_attack, cancellation immediately restarts the cycle
// (action_state moves back to Started, not left at Cancelled) - auto-attack
// has no "go idle and wait for a fresh BeginCast" state; the cycle is
// perpetual.
//
// Otherwise: ticks cooldown_remaining_ticks down by delta_ticks (saturating
// at 0). Still on cooldown after that (WeaponAction moves to Ongoing) is
// the ordinary "not ready yet" outcome - accepted as a no-op, the same
// accept-as-no-op precedent aura/pathing already establish for their own
// "nothing to do yet" cases.
//
// Once cooldown_remaining_ticks reaches 0 (WeaponAction moves back to
// Started - eligible, whether or not a swing actually lands this call), the
// rest - range, line of sight, and landing the hit - is delegated to
// attack_resolution::resolve_targeted_attack (shared with every other
// targeted attack or spell, not reimplemented here): attacker's
// WeaponAttack::min_range/max_range is the same single pair of numbers that
// covers melee and ranged alike (there is no separate "weapon type" concept
// here, see this capability's README section), and obstacle is EntityRef{}
// (is_null()) when there is nothing to check line of sight against - the
// same "unset" convention EntityRef's own doc comment describes.
// resolve_targeted_attack's own result is propagated unchanged: a reject
// (attacker/target has no movement::Position, or - once an attempt is
// actually made - target has no Health, health::on_apply_damage's own
// rejection) is a setup mistake surfaced as-is; an accepted-but-not-landed
// outcome (out of range, or blocked) is this function's own no-op - a
// fizzle, exactly like the cooldown case above, and cooldown_remaining_ticks
// is left at 0 (still eligible, waiting for conditions to become valid),
// not reset. Only once resolve_targeted_attack reports landed = true are
// pending_bonus_damage and cooldown_remaining_ticks reset and
// AutoAttackLanded published - a swing that didn't actually connect never
// consumes either, exactly as if it had never been attempted.
//
// Because these checks run only once off cooldown, an attacker/target
// missing movement::Position is not surfaced as a rejection while still on
// cooldown (there is nothing to resolve yet either way) - proven by
// TryAutoAttackAcceptsAsNoOpWhileOnCooldownEvenWithoutPositionSeeded.
//
// A separate, instant attack (a Sinister Strike-style ability) needs none
// of this mechanism at all: it is just another request dispatching
// health::on_apply_damage directly (or, per the same sharing rationale,
// attack_resolution::resolve_targeted_attack with no cooldown of its own),
// entirely independent of WeaponAttack's cooldown -
// auto_attack_test.cpp's InstantAttackBypassesTheAutoAttackCooldownEntirely
// proves this directly (WeaponAttack::cooldown_remaining_ticks is
// bit-for-bit unchanged by an ApplyDamage dispatched straight to health),
// not just asserts it in prose.
//
// Rejects if attacker has no WeaponAttack property.
[[nodiscard]] RequestResult
on_try_auto_attack(Context& ctx, ActionRegistry& registry, const TryAutoAttack& cmd);

// Cancellation, part one: movement, opt-in per weapon
// (WeaponAttack::requires_stationary, the same shape
// cast_time_attack::CastTimeAttack::requires_stationary uses for casts) -
// "some attacks require the attacker to stand still," not every weapon.
// Not a request handler - a subscriber meant to be registered against
// movement::PositionChanged by whoever composes this capability into a
// host (see demo/tests/simulated_host.hpp).
//
// Only queues a cancellation (atlas::runtime::request_cancel) - it does not
// reset the cooldown outright. The actual penalty is applied the next time
// on_try_auto_attack runs (via advance_action), matching "the runtime
// handles cancel first" as literal control flow rather than an out-of-band
// mutation the instant this event arrives.
//
// Only queues one when event.entity has a registry entry (i.e. has been
// given at least one TryAutoAttack call before - an entity that never has
// is simply irrelevant, not an error) and that weapon opted in
// (WeaponAttack::requires_stationary). An entity with no WeaponAttack
// property, or a weapon that didn't opt into requires_stationary, is left
// untouched.
void on_movement_occurred(Context& ctx, ActionRegistry& registry, const movement::PositionChanged& event);

// Cancellation, part two: interruption::ActionInterrupted, unconditional -
// the same generic mechanism cast_time_attack::on_action_interrupted
// responds to (see that capability's own doc comment and README section).
// Ignores requires_stationary entirely: a stun or disorient should
// interrupt any weapon's swing-in-progress, regardless of whether that
// particular weapon opted into caring about movement. Queues a
// cancellation exactly like on_movement_occurred does whenever
// event.entity has a registry entry; otherwise a no-op. Needs no Context
// at all - registry alone is enough to know whether this entity is this
// capability's concern.
void on_action_interrupted(ActionRegistry& registry, const interruption::ActionInterrupted& event);

} // namespace atlas::auto_attack
