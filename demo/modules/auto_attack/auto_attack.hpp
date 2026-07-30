#pragma once

// Generated at build time from
// demo/modules/auto_attack/auto_attack.capability.yaml (see
// demo/CMakeLists.txt) - the WeaponAttack/QueueAttackBonus/TryAutoAttack/
// AutoAttackLanded contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "attack_resolution/attack_resolution.hpp"
#include "auto_attack.capability.hpp"
#include "interruption/interruption.hpp"
#include "movement/movement.hpp"

namespace atlas::auto_attack {

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
// Ticks attacker's WeaponAttack::cooldown_remaining_ticks down by
// delta_ticks (saturating at 0, never wrapping negative on an unsigned
// field). Still on cooldown after that is the one "not ready yet" outcome
// this function checks itself - accepted as a no-op, not rejected, the same
// accept-as-no-op precedent aura/pathing already establish for their own
// "nothing to do yet" cases.
//
// Off cooldown, the rest - range, line of sight, and landing the hit - is
// delegated to attack_resolution::resolve_targeted_attack (shared with
// every other targeted attack or spell, not reimplemented here): attacker's
// WeaponAttack::min_range/max_range is the same single pair of numbers that
// covers melee and ranged alike (there is no separate "weapon type" concept
// here, see this capability's README section), and obstacle is EntityRef{}
// (is_null()) when there is nothing to check line of sight against - the
// same "unset" convention EntityRef's own doc comment describes.
// resolve_targeted_attack's own result is propagated unchanged: a reject
// (attacker/target has no movement::Position, or - once an attempt is
// actually made - target has no Health, health::on_apply_damage's own
// rejection) is a setup mistake surfaced as-is; an accepted-but-not-landed
// outcome (out of range, or blocked) is this function's own no-op, exactly
// like the cooldown case above. Only once resolve_targeted_attack reports
// landed = true are pending_bonus_damage and cooldown_remaining_ticks reset
// and AutoAttackLanded published - a swing that didn't actually connect
// never consumes either, exactly as if it had never been attempted.
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
[[nodiscard]] RequestResult on_try_auto_attack(Context& ctx, const TryAutoAttack& cmd);

// Cancellation, part one: movement, opt-in per weapon (WeaponAttack::requires_stationary,
// the same shape cast_time_attack::CastTimeAttack::requires_stationary uses
// for casts) - "some attacks require the attacker to stand still," not
// every weapon. Not a request handler - a subscriber meant to be registered
// against movement::PositionChanged by whoever composes this capability
// into a host (see demo/tests/simulated_host.hpp).
//
// Unlike cast_time_attack, there is no "in-progress attempt" for movement
// to cancel outright - a swing either lands or doesn't the instant
// on_try_auto_attack is called, there is nothing pending in between calls.
// What movement interrupts here instead is the *cooldown countdown*
// itself: if event.entity's WeaponAttack requires stationary and is
// currently mid-cycle (cooldown_remaining_ticks > 0 - the weapon is
// "winding up" toward its next swing), moving resets
// cooldown_remaining_ticks back to attack_speed_ticks, the same penalty a
// swing interrupted partway through would pay - waiting the full cycle
// over again, not shaving the remainder off. Already-ready
// (cooldown_remaining_ticks == 0) is left untouched: there is nothing
// in-progress to interrupt, so moving while ready to swing is never itself
// a penalty.
void on_movement_occurred(Context& ctx, const movement::PositionChanged& event);

// Cancellation, part two: interruption::ActionInterrupted, unconditional -
// the same generic mechanism cast_time_attack::on_action_interrupted
// responds to (see that capability's own doc comment and README section).
// Ignores requires_stationary entirely: a stun or disorient resets
// cooldown_remaining_ticks to attack_speed_ticks whenever it is currently
// mid-cycle (> 0), regardless of whether this particular weapon opted into
// caring about movement - being incapacitated interrupts any weapon's
// swing-in-progress. A no-op when already ready, for the same reason
// on_movement_occurred's is.
void on_action_interrupted(Context& ctx, const interruption::ActionInterrupted& event);

} // namespace atlas::auto_attack
