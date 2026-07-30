#include "auto_attack.hpp"

#include <cmath>
#include <cstdint>

namespace atlas::auto_attack {

RequestResult on_queue_attack_bonus(Context& ctx, const QueueAttackBonus& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto weapon = ctx.get<WeaponAttack>(cmd.attacker);
    if (!weapon) {
        return reject(cmd, "attacker has no WeaponAttack property");
    }

    weapon->get().pending_bonus_damage += cmd.bonus_damage;

    return accept(cmd);
}

RequestResult on_try_auto_attack(Context& ctx, const TryAutoAttack& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto weapon = ctx.get<WeaponAttack>(cmd.attacker);
    if (!weapon) {
        return reject(cmd, "attacker has no WeaponAttack property");
    }

    const auto attacker_position = ctx.get<movement::Position>(cmd.attacker);
    if (!attacker_position) {
        return reject(cmd, "attacker has no Position");
    }

    const auto target_position = ctx.get<movement::Position>(cmd.target);
    if (!target_position) {
        return reject(cmd, "target has no Position");
    }

    WeaponAttack& weapon_attack = weapon->get();
    weapon_attack.cooldown_remaining_ticks = weapon_attack.cooldown_remaining_ticks > cmd.delta_ticks
                                                 ? weapon_attack.cooldown_remaining_ticks - cmd.delta_ticks
                                                 : 0;

    if (weapon_attack.cooldown_remaining_ticks > 0) {
        return accept(cmd);
    }

    const float delta_x = target_position->get().x - attacker_position->get().x;
    const float delta_y = target_position->get().y - attacker_position->get().y;
    const float distance = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));

    if (distance < weapon_attack.min_range || distance > weapon_attack.max_range) {
        return accept(cmd);
    }

    if (!cmd.obstacle.is_null() &&
        line_of_sight::blocks_line_of_sight(ctx,
                                            line_of_sight::LineOfSightQuery{.obstacle = cmd.obstacle,
                                                                            .source = cmd.attacker,
                                                                            .target = cmd.target})) {
        return accept(cmd);
    }

    const std::int32_t total_damage = weapon_attack.damage + weapon_attack.pending_bonus_damage;

    // Propagated, not discarded: the same "a real internal dispatch, not a
    // parallel reimplementation" precedent pathing::on_advance_pathing
    // already establishes for movement::on_move. A target with no Health
    // property surfaces health::on_apply_damage's own rejection reason
    // unchanged - and, checked before the cooldown/pending_bonus_damage
    // reset below, a swing that didn't actually connect never consumes
    // either, exactly as if it had never been attempted.
    RequestResult damage_result =
        health::on_apply_damage(ctx, health::ApplyDamage{.target = cmd.target, .amount = total_damage});
    if (!damage_result.accepted) {
        return damage_result;
    }

    weapon_attack.pending_bonus_damage = 0;
    weapon_attack.cooldown_remaining_ticks = weapon_attack.attack_speed_ticks;

    ctx.publish<AutoAttackLanded>(AutoAttackLanded{
        .attacker = cmd.attacker,
        .target = cmd.target,
        .damage = total_damage,
    });

    return accept(cmd);
}

} // namespace atlas::auto_attack
