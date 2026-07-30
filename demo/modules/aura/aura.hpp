#pragma once

// Generated at build time from demo/modules/aura/aura.capability.yaml (see
// demo/CMakeLists.txt) - the AuraSource/ActivateAura/RefreshAuraEffect
// contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "aura.capability.hpp"
#include "movement/movement.hpp"

namespace atlas::aura {

// The manual implementation of ActivateAura's request handler (spec §14).
// Seeds source's declared range and multiplier - the one-time setup step
// every aura source needs before RefreshAuraEffect can do anything
// meaningful for it. A range of 0 is not a special case anywhere in this
// capability: it simply means no target other than source itself is ever
// within range (see on_refresh_aura_effect below), which is exactly how a
// self-only buff falls out of the same mechanism a zone effect uses, with
// no separate code path. Rejects if source has no AuraSource property
// seeded - a setup mistake, matching movement::on_move's precedent for
// Position/MovementSpeed (not every entity is expected to ever become an
// aura source, but one that is must have the property declared up front).
[[nodiscard]] RequestResult on_activate_aura(Context& ctx, const ActivateAura& cmd);

// The manual implementation of RefreshAuraEffect's request handler (spec
// §14) - the per-tick re-evaluation a range-based aura's contribution
// needs (spec §20, WhileCondition; see
// movement::refresh_speed_with_transient_contributions for why this can
// never be a stored, incrementally added/removed entry - nothing fires an
// event when a target walks out of range, since that's a fact about the
// current tick, not an occurrence). Computes the straight-line distance
// between source's and target's movement::Position; if it's within
// source's declared AuraSource::range, builds a single ephemeral
// Contribution<float> tagged runtime::Lifetime::WhileCondition and folds it
// into target's effective MovementSpeed via
// movement::refresh_speed_with_transient_contributions. Out of range, an
// empty span is passed instead, so target's effective MovementSpeed
// reflects only its stored Permanent contributions - exactly as if this
// aura had never touched it. There is no separate "the target left range"
// step and no removal call: every invocation recomputes the answer from
// scratch, driven by whoever calls RefreshAuraEffect (simulating a tick),
// never by a stored decision from a previous call. Rejects if source has
// no AuraSource property, or if either entity has no movement::Position.
[[nodiscard]] RequestResult on_refresh_aura_effect(Context& ctx,
                                                   movement::ContributionRegistry& speed_registry,
                                                   const RefreshAuraEffect& cmd);

} // namespace atlas::aura
