#include "damage_over_time.hpp"

namespace atlas::damage_over_time {

RequestResult on_apply_dot_effect(Context& ctx, const ApplyDotEffect& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto dot_effect = ctx.get<DotEffect>(cmd.target);
    if (!dot_effect) {
        return reject(cmd, "target has no DotEffect property");
    }

    DotEffect& effect = dot_effect->get();
    effect.damage_per_tick = cmd.damage_per_tick;
    effect.tick_interval_ticks = cmd.tick_interval_ticks;
    effect.ticks_until_next = cmd.tick_interval_ticks;
    effect.remaining_applications = cmd.total_applications;

    return accept(cmd);
}

RequestResult on_advance_dot_effect(Context& ctx, const AdvanceDotEffect& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto dot_effect = ctx.get<DotEffect>(cmd.target);
    if (!dot_effect) {
        return reject(cmd, "target has no DotEffect property");
    }

    DotEffect& effect = dot_effect->get();
    if (effect.remaining_applications == 0) {
        return accept(cmd);
    }

    effect.ticks_until_next =
        effect.ticks_until_next > cmd.delta_ticks ? effect.ticks_until_next - cmd.delta_ticks : 0;

    if (effect.ticks_until_next > 0) {
        return accept(cmd);
    }

    // Shared with every other capability that deals damage (see
    // attack_resolution's own callers), not reimplemented here - its
    // result is propagated unchanged.
    RequestResult damage_result = health::on_apply_damage(
        ctx, health::ApplyDamage{.target = cmd.target, .amount = effect.damage_per_tick});
    if (!damage_result.accepted) {
        return damage_result;
    }

    ctx.publish<DotEffectTicked>(DotEffectTicked{.target = cmd.target, .damage = effect.damage_per_tick});

    effect.remaining_applications -= 1;
    effect.ticks_until_next = effect.remaining_applications > 0 ? effect.tick_interval_ticks : 0;

    return accept(cmd);
}

} // namespace atlas::damage_over_time
