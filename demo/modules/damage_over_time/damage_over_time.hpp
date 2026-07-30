#pragma once

// Generated at build time from
// demo/modules/damage_over_time/damage_over_time.capability.yaml (see
// demo/CMakeLists.txt) - the
// DotEffect/ApplyDotEffect/AdvanceDotEffect/DotEffectTicked contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "damage_over_time.capability.hpp"
#include "health/health.hpp"

namespace atlas::damage_over_time {

// The manual implementation of ApplyDotEffect's request handler (spec
// §14) - seeds (or refreshes) target's DotEffect: damage_per_tick and
// tick_interval_ticks straight from the request, ticks_until_next reset to
// a full interval, and remaining_applications set to total_applications.
// There is no stacking (spec: single DotEffect slot per target, see this
// capability's own README section) - a fresh ApplyDotEffect on a target
// already mid-effect discards whatever was left of the previous one and
// starts over, the same "recomputed fresh, no separate remove step"
// precedent aura/haste already establish for their own WhileCondition
// effects, applied here to an ordinary discrete property instead.
//
// Rejects if target has no DotEffect property seeded.
[[nodiscard]] RequestResult on_apply_dot_effect(Context& ctx, const ApplyDotEffect& cmd);

// The manual implementation of AdvanceDotEffect's request handler (spec
// §14) - driven explicitly each call (delta_ticks simulation ticks elapsed
// since the last call), the same "caller simulates the tick" pattern
// auto_attack/aura/cast_time_attack already establish.
//
// A target with remaining_applications == 0 (never had an effect applied,
// or a previous one already ran to completion) is an ordinary no-op,
// accepted the same way auto_attack/aura/cast_time_attack's own "nothing
// to do yet" cases are. Otherwise: ticks_until_next counts down by
// delta_ticks (saturating at 0, the same pattern cast_time_attack's own
// remaining_ticks/auto_attack's own cooldown_remaining_ticks use). Not yet
// 0 is the ordinary "still waiting for the next tick" outcome. Once it
// reaches 0: dispatches health::on_apply_damage directly for
// damage_per_tick (an internal dispatch, not reimplemented here, the same
// precedent attack_resolution's own callers establish), publishes
// DotEffectTicked, and decrements remaining_applications - only if more
// applications remain is ticks_until_next reset to a fresh
// tick_interval_ticks; once exhausted, it's left at 0 and the effect goes
// idle (the same "0 remaining is the idle state" precedent
// CastAction/WeaponAction establish for their own lifecycles).
//
// Like pathing's own AdvancePathing (see its README section), a single
// delta_ticks step large enough to span more than one full
// tick_interval_ticks is not detected or corrected - at most one
// application fires per AdvanceDotEffect call, regardless of how large
// delta_ticks is; the caller is expected to advance in reasonably small
// steps, not skip whole intervals.
//
// Rejects if target has no DotEffect property, or (once a tick actually
// fires) if target has no Health - propagated unchanged from
// health::on_apply_damage.
[[nodiscard]] RequestResult on_advance_dot_effect(Context& ctx, const AdvanceDotEffect& cmd);

} // namespace atlas::damage_over_time
