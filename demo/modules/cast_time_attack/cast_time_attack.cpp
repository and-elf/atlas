#include "cast_time_attack.hpp"

namespace atlas::cast_time_attack {

RequestResult on_begin_cast(Context& ctx, const BeginCast& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto cast = ctx.get<CastTimeAttack>(cmd.caster);
    if (!cast) {
        return reject(cmd, "caster has no CastTimeAttack property");
    }

    CastTimeAttack& cast_time_attack = cast->get();
    if (cast_time_attack.is_casting) {
        return reject(cmd, "caster is already casting");
    }

    cast_time_attack.is_casting = true;
    cast_time_attack.requires_stationary = cmd.requires_stationary;
    cast_time_attack.target = cmd.target;
    cast_time_attack.obstacle = cmd.obstacle;
    cast_time_attack.min_range = cmd.min_range;
    cast_time_attack.max_range = cmd.max_range;
    cast_time_attack.damage = cmd.damage;
    cast_time_attack.cast_time_ticks = cmd.cast_time_ticks;
    cast_time_attack.remaining_ticks = cmd.cast_time_ticks;

    return accept(cmd);
}

RequestResult on_advance_cast(Context& ctx, const AdvanceCast& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto cast = ctx.get<CastTimeAttack>(cmd.caster);
    if (!cast) {
        return reject(cmd, "caster has no CastTimeAttack property");
    }

    CastTimeAttack& cast_time_attack = cast->get();
    if (!cast_time_attack.is_casting) {
        return accept(cmd);
    }

    cast_time_attack.remaining_ticks = cast_time_attack.remaining_ticks > cmd.delta_ticks
                                           ? cast_time_attack.remaining_ticks - cmd.delta_ticks
                                           : 0;

    if (cast_time_attack.remaining_ticks > 0) {
        return accept(cmd);
    }

    cast_time_attack.is_casting = false;

    // Shared with auto_attack (see attack_resolution's own README section
    // for why), not reimplemented here - its result is propagated
    // unchanged, the same "a real internal dispatch, not a parallel
    // reimplementation" precedent pathing::on_advance_pathing already
    // establishes for movement::on_move.
    const attack_resolution::TargetedAttackOutcome outcome =
        attack_resolution::resolve_targeted_attack(ctx,
                                                   attack_resolution::TargetedAttackQuery{
                                                       .attacker = cmd.caster,
                                                       .target = cast_time_attack.target,
                                                       .obstacle = cast_time_attack.obstacle,
                                                       .min_range = cast_time_attack.min_range,
                                                       .max_range = cast_time_attack.max_range,
                                                       .damage = cast_time_attack.damage,
                                                   });

    if (!outcome.result.accepted) {
        return outcome.result;
    }

    if (outcome.landed) {
        ctx.publish<CastLanded>(CastLanded{
            .caster = cmd.caster,
            .target = cast_time_attack.target,
            .damage = cast_time_attack.damage,
        });
    }

    return accept(cmd);
}

void on_movement_occurred(Context& ctx, const movement::PositionChanged& event) {
    auto cast = ctx.get<CastTimeAttack>(event.target);
    if (!cast) {
        return;
    }

    CastTimeAttack& cast_time_attack = cast->get();
    if (!cast_time_attack.is_casting || !cast_time_attack.requires_stationary) {
        return;
    }

    cast_time_attack.is_casting = false;
    cast_time_attack.remaining_ticks = 0;
}

void on_action_interrupted(Context& ctx, const interruption::ActionInterrupted& event) {
    auto cast = ctx.get<CastTimeAttack>(event.entity);
    if (!cast) {
        return;
    }

    CastTimeAttack& cast_time_attack = cast->get();
    if (!cast_time_attack.is_casting) {
        return;
    }

    cast_time_attack.is_casting = false;
    cast_time_attack.remaining_ticks = 0;
}

} // namespace atlas::cast_time_attack
