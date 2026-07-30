#include "haste.hpp"

#include "atlas/runtime/property_composition.hpp"

#include <array>
#include <cmath>
#include <span>

namespace atlas::haste {

RequestResult on_activate_haste(Context& ctx, const ActivateHaste& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto haste_source = ctx.get<HasteSource>(cmd.source);
    if (!haste_source) {
        return reject(cmd, "source has no HasteSource property");
    }

    haste_source->get().range = cmd.range;
    haste_source->get().multiplier = cmd.multiplier;

    return accept(cmd);
}

RequestResult on_refresh_haste_effect(Context& ctx,
                                      runtime::PropertyStore<cast_time_attack::CastSpeed>& cast_speed_store,
                                      const cast_time_attack::CastSpeedRegistry& cast_speed_registry,
                                      const RefreshHasteEffect& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    const auto haste_source = ctx.get<HasteSource>(cmd.source);
    if (!haste_source) {
        return reject(cmd, "source has no HasteSource property");
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
    // documentation of why it exists only for this one call (spec §20; see
    // property_composition.hpp's Lifetime and
    // cast_time_attack::refresh_cast_speed_with_transient_contributions) -
    // never written anywhere that would outlive this function call.
    const std::array<runtime::Contribution<float>, 1> in_range_contribution{
        {{.source = "haste",
          .value = haste_source->get().multiplier,
          .lifetime = runtime::Lifetime::WhileCondition}}};

    std::span<const runtime::Contribution<float>> transient;
    if (distance <= static_cast<float>(haste_source->get().range)) {
        transient = in_range_contribution;
    }

    cast_time_attack::refresh_cast_speed_with_transient_contributions(
        cast_speed_store, cast_speed_registry, cmd.target, transient);

    return accept(cmd);
}

} // namespace atlas::haste
