#pragma once

// Generated at build time from demo/modules/haste/haste.capability.yaml (see
// demo/CMakeLists.txt) - the HasteSource/CastSpeed/ActivateHaste/
// RefreshHasteEffect contracts, with CastSpeed's composition: Multiplicative
// strategy declared.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/property_store.hpp"

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
// HasteSource::range, writes source's multiplier straight into target's
// CastSpeed - otherwise 1.0 (CastSpeed's own declared identity), so
// target's effective CastSpeed falls back to exactly what it would be
// without this haste source. Driven explicitly each call by whoever
// simulates a tick, the same pattern aura::on_refresh_aura_effect already
// establishes - there is no separate "target left range" step, and no
// registry of stored (Permanent) contributions: haste is CastSpeed's only
// contributor today, so resolving through
// atlas::runtime::resolve_multiplicative over a one-entry span would add
// ceremony without changing the result - see movement's own
// ContributionRegistry precedent for when that stops being true (a second,
// independent contributor) and this would gain one too.
//
// Takes cast_speed_store directly (not just Context) because
// Context::get<T> only mutates an entry that already exists, never creates
// one - a target's first-ever haste effect is exactly the moment CastSpeed
// starts existing for them, and there is no separate "declare CastSpeed"
// request a target opts into first (contrast BeginCast, which does declare
// CastTimeAttack up front).
//
// Rejects if source has no HasteSource property, or if either entity has
// no movement::Position.
[[nodiscard]] RequestResult on_refresh_haste_effect(Context& ctx,
                                                    runtime::PropertyStore<CastSpeed>& cast_speed_store,
                                                    const RefreshHasteEffect& cmd);

} // namespace atlas::haste
