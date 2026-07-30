#include "haste.hpp"

#include <cmath>

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
                                      runtime::PropertyStore<CastSpeed>& cast_speed_store,
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

    const float multiplier =
        distance <= static_cast<float>(haste_source->get().range) ? haste_source->get().multiplier : 1.0F;
    cast_speed_store.set(cmd.target, CastSpeed{.base = multiplier});

    return accept(cmd);
}

} // namespace atlas::haste
