#pragma once

// Generated at build time from
// demo/modules/attack_resolution/attack_resolution.capability.yaml (see
// demo/CMakeLists.txt) - a deliberately empty generated contract. This
// capability declares no properties/requests/events of its own (a name-only
// manifest is a legal, tooling-supported shape - see this directory's
// README for why); it exists purely to host resolve_targeted_attack, the
// range + line-of-sight + damage sequence every targeted attack or spell
// shares. Factored out of auto_attack::on_try_auto_attack once it became
// clear a second caller (a future instant-attack request, a future spell
// cast) would need the identical sequence - the same "share the mechanism,
// don't duplicate it" precedent pathing::on_advance_pathing already set for
// movement::on_move.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include <cstdint>

#include "attack_resolution.capability.hpp"

namespace atlas::attack_resolution {

// A named parameter bundle (see line_of_sight::LineOfSightQuery precedent).
// min_range/max_range/damage are plain values, not read from any one
// capability's own property here: a weapon-based auto-attack sources them
// from auto_attack::WeaponAttack, while a future spell would source its own
// range/damage from its own property instead - this capability only cares
// about the resolved values, never where they came from.
struct TargetedAttackQuery {
    EntityRef attacker;
    EntityRef target;
    EntityRef obstacle;
    std::int32_t min_range = 0;
    std::int32_t max_range = 0;
    std::int32_t damage = 0;
};

// The outcome of one resolution attempt.
//
// result is exactly what the caller's own request handler should return -
// atlas::accept/atlas::reject (constructed against query, which - like
// line_of_sight::LineOfSightQuery - is itself a plain aggregate and so
// already satisfies atlas::RequestContract) when this function's own
// validation fails, or health::on_apply_damage's own result, propagated
// unchanged, once an attack is actually attempted. Never reconstructed from
// scratch - the same "propagate, don't discard" precedent already
// established for health::on_apply_damage's own [[nodiscard]] result.
//
// landed is false whenever result.accepted is true but nothing actually
// happened this call (out of range, or line of sight blocked) - a valid,
// non-erroneous no-op the caller must not mistake for a successful attack.
// It is true only once health::on_apply_damage has actually accepted the
// damage; the caller decides for itself what "landed" means for its own
// state (auto_attack resets its cooldown and consumes pending_bonus_damage
// only then - a future instant-attack request may have nothing further to
// do at all).
struct TargetedAttackOutcome {
    RequestResult result;
    bool landed = false;
};

// Resolves one targeted attack attempt: query.attacker and query.target
// must both have a movement::Position (reject, not silently skip, if
// either is missing - an entity that can't be positioned was never meant to
// attack or be attacked); the distance between them must fall within
// [query.min_range, query.max_range] and, when query.obstacle is not
// EntityRef::is_null(), line_of_sight::blocks_line_of_sight must return
// false - otherwise the attempt is a valid no-op (landed = false, result
// accepted). Only once both checks pass is query.damage applied to
// query.target via health::on_apply_damage, whose own result is propagated
// unchanged.
[[nodiscard]] TargetedAttackOutcome resolve_targeted_attack(Context& ctx, const TargetedAttackQuery& query);

} // namespace atlas::attack_resolution
