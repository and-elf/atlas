#include "attack_resolution.hpp"

#include <cmath>

#include "health/health.hpp"
#include "line_of_sight/line_of_sight.hpp"
#include "movement/movement.hpp"

namespace atlas::attack_resolution {

TargetedAttackOutcome resolve_targeted_attack(Context& ctx, const TargetedAttackQuery& query) {
    const auto attacker_position = ctx.get<movement::Position>(query.attacker);
    if (!attacker_position) {
        return TargetedAttackOutcome{.result = reject(query, "attacker has no Position"), .landed = false};
    }

    const auto target_position = ctx.get<movement::Position>(query.target);
    if (!target_position) {
        return TargetedAttackOutcome{.result = reject(query, "target has no Position"), .landed = false};
    }

    const float delta_x = target_position->get().x - attacker_position->get().x;
    const float delta_y = target_position->get().y - attacker_position->get().y;
    const float distance = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));

    if (distance < static_cast<float>(query.min_range) || distance > static_cast<float>(query.max_range)) {
        return TargetedAttackOutcome{.result = accept(query), .landed = false};
    }

    if (!query.obstacle.is_null() &&
        line_of_sight::blocks_line_of_sight(ctx,
                                            line_of_sight::LineOfSightQuery{.obstacle = query.obstacle,
                                                                            .source = query.attacker,
                                                                            .target = query.target})) {
        return TargetedAttackOutcome{.result = accept(query), .landed = false};
    }

    // Propagated, not discarded: the same "a real internal dispatch, not a
    // parallel reimplementation" precedent already established elsewhere in
    // this demo (pathing::on_advance_pathing calling movement::on_move).
    RequestResult damage_result =
        health::on_apply_damage(ctx, health::ApplyDamage{.target = query.target, .amount = query.damage});
    return TargetedAttackOutcome{.result = damage_result, .landed = damage_result.accepted};
}

} // namespace atlas::attack_resolution
