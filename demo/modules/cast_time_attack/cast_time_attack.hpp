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

} // namespace atlas::cast_time_attack
