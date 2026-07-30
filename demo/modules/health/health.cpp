#include "health.hpp"

// Armor's generated CONTRACT only - a tiny-interface structural dependency
// (spec §5) on another capability's property shape, not a dependency on
// armor's implementation (armor.hpp/armor.cpp are never included here).
#include <algorithm>
#include <functional>

#include "armor.capability.hpp"

namespace atlas::health {

RequestResult on_apply_damage(Context& ctx, const ApplyDamage& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    return ctx.get<Health>(cmd.target)
        .transform([&](std::reference_wrapper<Health> health_ref) -> RequestResult {
            Health& health = health_ref.get();

            // Armor mitigates incoming damage (spec §20's own Additive
            // example, "Armor: 100 + 50 + 20 = 170", applied here as
            // mitigation rather than accumulation - same composition
            // mechanism, a different arithmetic use of the composed
            // value). An entity with no Armor property at all mitigates
            // nothing, rather than being treated as a request-rejecting
            // error - not every damageable entity needs to compose armor.
            const auto armor = ctx.get<armor::Armor>(cmd.target);
            const std::int32_t mitigation = armor ? armor->get().base : 0;
            const std::int32_t effective_damage = std::max(std::int32_t{0}, cmd.amount - mitigation);

            health.current = std::clamp(health.current - effective_damage, 0, health.maximum);

            ctx.publish<HealthChanged>(HealthChanged{
                .target = cmd.target,
                .new_current = health.current,
            });

            return accept(cmd);
        })
        .value_or(reject(cmd, "target has no Health"));
}

void write_health(serialization::ByteWriter& writer, const Health& health) {
    writer.write_i32(health.current);
    writer.write_i32(health.maximum);
}

std::optional<Health> read_health(serialization::ByteReader& reader) {
    const auto current = reader.read_i32();
    if (!current) {
        return std::nullopt;
    }
    const auto maximum = reader.read_i32();
    if (!maximum) {
        return std::nullopt;
    }
    return Health{.current = *current, .maximum = *maximum};
}

} // namespace atlas::health
