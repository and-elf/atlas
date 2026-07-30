#pragma once

// Generated at build time from demo/modules/haste/haste.capability.yaml (see
// demo/CMakeLists.txt) - the HasteSource/ActivateHaste/RefreshHasteEffect
// contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/property_store.hpp"

#include "cast_time_attack/cast_time_attack.hpp"
#include "haste.capability.hpp"
#include "movement/movement.hpp"

namespace atlas::haste {

// The manual implementation of ActivateHaste's request handler (spec §14) -
// the CastSpeed analogue of aura::on_activate_aura for MovementSpeed. Seeds
// source's declared range and multiplier, the one-time setup step every
// haste source needs before RefreshHasteEffect can do anything meaningful
// for it. Rejects if source has no HasteSource property seeded.
[[nodiscard]] RequestResult on_activate_haste(Context& ctx, const ActivateHaste& cmd);

// The manual implementation of RefreshHasteEffect's request handler (spec
// §14) - the CastSpeed analogue of aura::on_refresh_aura_effect: computes
// the straight-line distance between source's and target's
// movement::Position, and if it's within source's declared
// HasteSource::range, folds an ephemeral WhileCondition Contribution<float>
// (spec §20) carrying source's multiplier into target's effective CastSpeed
// via cast_time_attack::refresh_cast_speed_with_transient_contributions -
// otherwise an empty span, so target's effective CastSpeed falls back to
// exactly what it would be without this haste source (movement::Position's
// own README/aura section covers why this is never a stored, incrementally
// added/removed contribution). Driven explicitly each call by whoever
// simulates a tick, the same pattern aura::on_refresh_aura_effect already
// establishes - there is no separate "target left range" step.
//
// Takes cast_speed_store directly (not just Context) for the same reason
// cast_time_attack::refresh_cast_speed_with_transient_contributions does -
// a target's first-ever haste effect is exactly the moment CastSpeed starts
// existing for them, and Context::get<T> alone can never create that entry.
//
// Rejects if source has no HasteSource property, or if either entity has
// no movement::Position.
[[nodiscard]] RequestResult
on_refresh_haste_effect(Context& ctx,
                        runtime::PropertyStore<cast_time_attack::CastSpeed>& cast_speed_store,
                        const cast_time_attack::CastSpeedRegistry& cast_speed_registry,
                        const RefreshHasteEffect& cmd);

} // namespace atlas::haste
