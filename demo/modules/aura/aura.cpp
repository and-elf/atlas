#include "aura.hpp"

#include "atlas/runtime/property_composition.hpp"

#include <array>
#include <cmath>
#include <span>

namespace atlas::aura {

RequestResult on_activate_aura(Context& ctx, const ActivateAura& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto aura_source = ctx.get<AuraSource>(cmd.source);
    if (!aura_source) {
        return reject(cmd, "source has no AuraSource property");
    }

    aura_source->get().range = cmd.range;
    aura_source->get().multiplier = cmd.multiplier;

    return accept(cmd);
}

RequestResult on_refresh_aura_effect(Context& ctx,
                                     movement::ContributionRegistry& speed_registry,
                                     const RefreshAuraEffect& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    const auto aura_source = ctx.get<AuraSource>(cmd.source);
    if (!aura_source) {
        return reject(cmd, "source has no AuraSource property");
    }

    const auto source_position = ctx.get<movement::Position>(cmd.source);
    if (!source_position) {
        return reject(cmd, "source has no Position");
    }

    const auto target_position = ctx.get<movement::Position>(cmd.target);
    if (!target_position) {
        return reject(cmd, "target has no Position");
    }

    const float delta_x = target_position->get().x - source_position->get().x;
    const float delta_y = target_position->get().y - source_position->get().y;
    const float distance = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));

    // A fresh ephemeral contribution, tagged WhileCondition purely as
    // documentation of why it exists only for this one call (spec §20;
    // see property_composition.hpp's Lifetime and movement.hpp's
    // refresh_speed_with_transient_contributions) - never written anywhere
    // that would outlive this function call.
    const std::array<runtime::Contribution<float>, 1> in_range_contribution{
        {{.source = "aura",
          .value = aura_source->get().multiplier,
          .lifetime = runtime::Lifetime::WhileCondition}}};

    std::span<const runtime::Contribution<float>> transient;
    if (distance <= aura_source->get().range) {
        transient = in_range_contribution;
    }

    movement::refresh_speed_with_transient_contributions(ctx, speed_registry, cmd.target, transient);

    return accept(cmd);
}

} // namespace atlas::aura
