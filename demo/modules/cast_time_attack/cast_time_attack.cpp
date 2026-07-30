#include "cast_time_attack.hpp"

#include <cmath>
#include <cstdint>

namespace atlas::cast_time_attack {

RequestResult on_begin_cast(Context& ctx, ActionRegistry& registry, const BeginCast& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto cast = ctx.get<CastTimeAttack>(cmd.caster);
    if (!cast) {
        return reject(cmd, "caster has no CastTimeAttack property");
    }

    CastAction& action = registry[cmd.caster];
    if (action.action_state == runtime::ActionState::Started ||
        action.action_state == runtime::ActionState::Ongoing) {
        return reject(cmd, "caster is already casting");
    }

    // Resolved once, here, and locked in for the whole cast (see this
    // function's own doc comment in cast_time_attack.hpp for why it's never
    // re-resolved mid-cast). A non-positive multiplier is nonsensical (an
    // authoring mistake, not a real haste value) and would otherwise divide
    // by zero or a negative number below - guarded against here rather than
    // trusted, since a bad HasteSource::multiplier is exactly the kind of
    // input this validation step exists to catch before it reaches
    // undefined behavior.
    const auto cast_speed = ctx.get<haste::CastSpeed>(cmd.caster);
    const float cast_speed_multiplier =
        (cast_speed && cast_speed->get().base > 0.0F) ? cast_speed->get().base : 1.0F;
    const auto effective_ticks = static_cast<std::uint64_t>(
        std::llround(static_cast<double>(cmd.cast_time_ticks) / static_cast<double>(cast_speed_multiplier)));

    CastTimeAttack& cast_time_attack = cast->get();
    cast_time_attack.requires_stationary = cmd.requires_stationary;
    cast_time_attack.target = cmd.target;
    cast_time_attack.obstacle = cmd.obstacle;
    cast_time_attack.min_range = cmd.min_range;
    cast_time_attack.max_range = cmd.max_range;
    cast_time_attack.damage = cmd.damage;
    cast_time_attack.cast_time_ticks = effective_ticks;
    cast_time_attack.remaining_ticks = effective_ticks;
    cast_time_attack.animation = cmd.animation;

    action.action_state = runtime::ActionState::Started;
    action.cancel_requested = false;

    ctx.publish<CastStarted>(CastStarted{
        .caster = cmd.caster,
        .animation = cmd.animation,
        .duration_ticks = effective_ticks,
    });

    return accept(cmd);
}

RequestResult on_advance_cast(Context& ctx, ActionRegistry& registry, const AdvanceCast& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto cast = ctx.get<CastTimeAttack>(cmd.caster);
    if (!cast) {
        return reject(cmd, "caster has no CastTimeAttack property");
    }

    CastTimeAttack& cast_time_attack = cast->get();
    bool completed_this_call = false;

    runtime::advance_action(
        registry[cmd.caster],
        [&](CastAction&) {
            // Cancelled - the wind-up is over, nothing to resolve.
            cast_time_attack.remaining_ticks = 0;
        },
        [&](CastAction& action) {
            cast_time_attack.remaining_ticks = cast_time_attack.remaining_ticks > cmd.delta_ticks
                                                   ? cast_time_attack.remaining_ticks - cmd.delta_ticks
                                                   : 0;

            if (cast_time_attack.remaining_ticks > 0) {
                action.action_state = runtime::ActionState::Ongoing;
                return;
            }

            action.action_state = runtime::ActionState::Completed;
            completed_this_call = true;
        });

    if (!completed_this_call) {
        return accept(cmd);
    }

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

void on_movement_occurred(Context& ctx, ActionRegistry& registry, const movement::PositionChanged& event) {
    auto action_it = registry.find(event.target);
    if (action_it == registry.end()) {
        return;
    }

    auto cast = ctx.get<CastTimeAttack>(event.target);
    if (!cast || !cast->get().requires_stationary) {
        return;
    }

    runtime::request_cancel(action_it->second);
}

void on_action_interrupted(ActionRegistry& registry, const interruption::ActionInterrupted& event) {
    auto action_it = registry.find(event.entity);
    if (action_it == registry.end()) {
        return;
    }

    runtime::request_cancel(action_it->second);
}

} // namespace atlas::cast_time_attack
