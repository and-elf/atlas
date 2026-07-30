#pragma once

// Generated at build time from
// demo/modules/cast_time_attack/cast_time_attack.capability.yaml (see
// demo/CMakeLists.txt) - the CastTimeAttack/BeginCast/AdvanceCast/CastLanded
// contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "attack_resolution/attack_resolution.hpp"
#include "cast_time_attack.capability.hpp"
#include "interruption/interruption.hpp"
#include "movement/movement.hpp"

namespace atlas::cast_time_attack {

// The manual implementation of BeginCast's request handler (spec §14) -
// starts a wind-up: sets is_casting/target/obstacle/min_range/max_range/
// damage from the request onto caster's CastTimeAttack property and resets
// remaining_ticks to cast_time_ticks. Deliberately checks nothing about
// range or line of sight here - that validation happens once, at
// AdvanceCast's completion (see below), not at the start; a caster may
// begin casting at a target that's currently out of range, hoping to close
// the distance before the cast finishes.
//
// is_casting, not "remaining_ticks > 0", is what distinguishes an active
// cast from an idle caster: a 0-cast-time ability starts with
// remaining_ticks already at 0 (see on_advance_cast below), which would be
// indistinguishable from "never cast anything" if remaining_ticks were the
// only signal.
//
// Rejects if caster has no CastTimeAttack property, or if caster is already
// casting (is_casting already true from a previous, not-yet-complete
// BeginCast) - there is no instant-cancel-and-restart in this capability
// (see this capability's README section for the scope cut).
[[nodiscard]] RequestResult on_begin_cast(Context& ctx, const BeginCast& cmd);

// The manual implementation of AdvanceCast's request handler (spec §14) -
// driven explicitly each call (delta_ticks simulation ticks elapsed since
// the last call), the same "caller simulates the tick" pattern
// auto_attack::on_try_auto_attack/aura::on_refresh_aura_effect already
// establish.
//
// !is_casting on entry (never cast, or a previous cast already resolved) is
// a no-op - there is nothing to advance. Otherwise ticks caster's
// CastTimeAttack::remaining_ticks down by delta_ticks (saturating at 0);
// not yet 0 after that is the ordinary "still casting" outcome - accepted
// as a no-op, the same accept-as-no-op precedent auto_attack/aura/pathing
// already establish for their own "nothing to do yet" cases.
//
// Once remaining_ticks reaches 0 this call, is_casting resets to false (the
// wind-up is over, whether or not the attack actually connects - see
// below) and the cast resolves: delegates to
// attack_resolution::resolve_targeted_attack (shared with auto_attack, not
// reimplemented here) using the target/obstacle/min_range/max_range/damage
// BeginCast locked in. Its result is propagated unchanged - a reject
// (caster/target has no movement::Position, or target has no Health once
// an attempt is actually made) surfaces as-is; an accepted-but-not-landed
// outcome (target moved out of range, or line of sight is now blocked) is a
// fizzle - there is no lingering "waiting for range" state, and CastLanded
// is published only when resolve_targeted_attack reports landed = true.
//
// Rejects if caster has no CastTimeAttack property.
[[nodiscard]] RequestResult on_advance_cast(Context& ctx, const AdvanceCast& cmd);

// Cancellation, part one: movement, opt-in per cast. Not a request handler -
// a subscriber meant to be registered against movement::PositionChanged by
// whoever composes this capability into a host (see demo/tests/simulated_host.hpp),
// the same way a capability's own request handlers are registered against a
// request::Dispatcher rather than called directly. If event.entity is
// currently casting (is_casting) and that specific cast opted in
// (requires_stationary, set at BeginCast time - "some attacks require the
// caster to stand still," not every attack), the cast is cancelled outright:
// is_casting resets to false, remaining_ticks resets to 0, no
// attack_resolution::resolve_targeted_attack attempt is made, no CastLanded
// is published. An entity with no CastTimeAttack property, or one that
// isn't currently casting, or a cast that didn't opt into
// requires_stationary, is left untouched - this is deliberately not a
// request, so there is nothing to reject; an event that doesn't apply here
// is simply ignored, the same "harmless, not an error" case ctx.publish<T>()
// itself already documents for a nobody-subscribed event.
void on_movement_occurred(Context& ctx, const movement::PositionChanged& event);

// Cancellation, part two: interruption::ActionInterrupted, unconditional.
// The generic mechanism a crowd-control effect (a stun, a disorient - not
// yet built in this demo, see this capability's README section) would
// trigger: unlike on_movement_occurred above, this ignores
// requires_stationary entirely - being stunned interrupts a cast regardless
// of whether that specific cast cared about the caster's own movement.
// Cancels exactly like on_movement_occurred does (is_casting = false,
// remaining_ticks = 0, no resolution attempt, no CastLanded) whenever
// event.entity is currently casting; otherwise a no-op.
void on_action_interrupted(Context& ctx, const interruption::ActionInterrupted& event);

} // namespace atlas::cast_time_attack
