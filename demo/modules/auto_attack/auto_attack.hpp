#pragma once

// Generated at build time from
// demo/modules/auto_attack/auto_attack.capability.yaml (see
// demo/CMakeLists.txt) - the WeaponAttack/QueueAttackBonus/TryAutoAttack/
// AutoAttackLanded contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "auto_attack.capability.hpp"
#include "health/health.hpp"
#include "line_of_sight/line_of_sight.hpp"
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
// field). The swing does not land this call - though the request is still
// *accepted*, none of these are setup mistakes - when: still on cooldown;
// or target's distance from attacker's current movement::Position falls
// outside [min_range, max_range] (the same single pair of numbers covers
// melee and ranged alike - there is no separate "weapon type" concept
// here, see this capability's README section); or obstacle (when not
// EntityRef::is_null()) blocks line of sight between attacker and target
// (line_of_sight::blocks_line_of_sight). All three are the ordinary
// "not ready to land yet" outcome a caller re-issuing this every tick will
// see most of the time - the same accept-as-no-op precedent aura/pathing
// already establish for their own "nothing to do yet" cases, not a
// rejection.
//
// obstacle is EntityRef{} (is_null()) when there is nothing to check line
// of sight against: line_of_sight checks exactly one named obstacle per
// call (PropertyStore<T> has no iteration - see its own README section), so
// a caller with no relevant obstacle in the current scene passes the null
// sentinel to skip the check entirely, the same "unset" convention
// EntityRef's own doc comment already describes.
//
// When the swing does land: internally dispatches health::on_apply_damage
// with attacker's WeaponAttack::damage plus pending_bonus_damage, its
// result propagated unchanged rather than discarded - the same "a real
// internal dispatch, not a parallel reimplementation" precedent
// pathing::on_advance_pathing already establishes for movement::on_move,
// so a target with no Health property surfaces health's own rejection
// reason here too. Only once that dispatch actually accepts are both
// consumed (pending_bonus_damage resets to 0, cooldown_remaining_ticks
// resets to attack_speed_ticks) and AutoAttackLanded published - a swing
// that didn't actually connect never consumes either, exactly as if it had
// never been attempted. A separate, instant attack (a Sinister Strike-style
// ability) needs none of this mechanism at all: it is just another request
// dispatching health::on_apply_damage directly, entirely independent of
// WeaponAttack's cooldown - auto_attack_test.cpp's
// InstantAttackBypassesTheAutoAttackCooldownEntirely proves this directly
// (WeaponAttack::cooldown_remaining_ticks is bit-for-bit unchanged by an
// ApplyDamage dispatched straight to health), not just asserts it in prose.
//
// Rejects if attacker has no WeaponAttack property, or if either attacker
// or target has no movement::Position - setup mistakes, not ordinary
// "not ready" outcomes.
[[nodiscard]] RequestResult on_try_auto_attack(Context& ctx, const TryAutoAttack& cmd);

} // namespace atlas::auto_attack
