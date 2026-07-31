#include "auto_attack.hpp"

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

RequestResult on_try_auto_attack(Context& ctx, ActionRegistry& registry, const TryAutoAttack& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto weapon = ctx.get<WeaponAttack>(cmd.attacker);
    if (!weapon) {
        return reject(cmd, "attacker has no WeaponAttack property");
    }

    WeaponAttack& weapon_attack = weapon->get();
    WeaponAction& weapon_action = registry[cmd.attacker];

    // Attacker's own scheduled turn to notice a pending cancellation (spec
    // §20, Triggered composition) - ordinary consumes-shaped reads, absent
    // (nullopt) unless movement/interruption occurred since the last call.
    if (weapon_attack.requires_stationary && ctx.get<movement::PositionChanged>(cmd.attacker).has_value()) {
        runtime::request_cancel(weapon_action);
    }
    if (ctx.get<interruption::ActionInterrupted>(cmd.attacker).has_value()) {
        runtime::request_cancel(weapon_action);
    }

    bool eligible_this_call = false;

    runtime::advance_action(
        weapon_action,
        [&](WeaponAction& action) {
            // Cancelled: interrupted mid-cycle - full-cycle penalty, unless
            // there was nothing in progress to interrupt in the first
            // place. The cycle is perpetual, so it restarts immediately
            // rather than staying Cancelled.
            if (weapon_attack.cooldown_remaining_ticks > 0) {
                weapon_attack.cooldown_remaining_ticks = weapon_attack.attack_speed_ticks;
            }
            action.action_state = runtime::ActionState::Started;
        },
        [&](WeaponAction& action) {
            weapon_attack.cooldown_remaining_ticks =
                weapon_attack.cooldown_remaining_ticks > cmd.delta_ticks
                    ? weapon_attack.cooldown_remaining_ticks - cmd.delta_ticks
                    : 0;

            if (weapon_attack.cooldown_remaining_ticks > 0) {
                action.action_state = runtime::ActionState::Ongoing;
                return;
            }

            action.action_state = runtime::ActionState::Started;
            eligible_this_call = true;
        });

    if (!eligible_this_call) {
        return accept(cmd);
    }

    const std::int32_t total_damage = weapon_attack.damage + weapon_attack.pending_bonus_damage;

    // Shared with every other targeted attack or spell (see
    // attack_resolution's own README section for why), not reimplemented
    // here - its result is propagated unchanged, the same "a real internal
    // dispatch, not a parallel reimplementation" precedent
    // pathing::on_advance_pathing already establishes for movement::on_move.
    const attack_resolution::TargetedAttackOutcome outcome =
        attack_resolution::resolve_targeted_attack(ctx,
                                                   attack_resolution::TargetedAttackQuery{
                                                       .attacker = cmd.attacker,
                                                       .target = cmd.target,
                                                       .obstacle = cmd.obstacle,
                                                       .min_range = weapon_attack.min_range,
                                                       .max_range = weapon_attack.max_range,
                                                       .damage = total_damage,
                                                   });

    if (!outcome.result.accepted) {
        return outcome.result;
    }

    if (!outcome.landed) {
        return accept(cmd);
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
