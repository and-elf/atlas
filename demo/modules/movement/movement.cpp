#include "movement.hpp"

#include "atlas/core/time.hpp"

#include <functional>
#include <stdexcept>

namespace atlas::movement {

void set_base_speed(Context& ctx, ContributionRegistry& registry, EntityRef entity, float base_speed) {
    auto movement_speed = ctx.get<MovementSpeed>(entity);
    if (!movement_speed) {
        throw std::logic_error("atlas::movement::set_base_speed: entity has no MovementSpeed property");
    }

    registry[entity] = SpeedContributions{.declared_base = base_speed, .contributions = {}};

    // With zero contributions, resolve_multiplicative's fold degenerates to
    // the base itself - written here directly rather than through
    // resolve_multiplicative(base_speed, {}) purely to avoid a redundant
    // call with an empty span; the result is identical either way.
    movement_speed->get().base = base_speed;
}

void add_speed_contribution(Context& ctx,
                            ContributionRegistry& registry,
                            EntityRef entity,
                            std::string_view source,
                            float multiplier) {
    auto movement_speed = ctx.get<MovementSpeed>(entity);
    if (!movement_speed) {
        throw std::logic_error(
            "atlas::movement::add_speed_contribution: entity has no MovementSpeed property");
    }

    const auto registry_it = registry.find(entity);
    if (registry_it == registry.end()) {
        throw std::logic_error(
            "atlas::movement::add_speed_contribution: entity has no base speed seeded via set_base_speed");
    }

    SpeedContributions& speed_contributions = registry_it->second;
    speed_contributions.contributions.push_back(
        runtime::Contribution<float>{.source = source, .value = multiplier});

    // Resolved from speed_contributions.declared_base, never from
    // movement_speed->get().base - the latter already holds the *previous*
    // resolution's output, not the entity's original declared base (see
    // movement.hpp's SpeedContributions comment for why that distinction is
    // load-bearing for Multiplicative specifically).
    movement_speed->get().base = runtime::resolve_multiplicative<float>(speed_contributions.declared_base,
                                                                        speed_contributions.contributions);
}

void remove_speed_contribution(Context& ctx,
                               ContributionRegistry& registry,
                               EntityRef entity,
                               std::string_view source) {
    auto movement_speed = ctx.get<MovementSpeed>(entity);
    if (!movement_speed) {
        throw std::logic_error(
            "atlas::movement::remove_speed_contribution: entity has no MovementSpeed property");
    }

    const auto registry_it = registry.find(entity);
    if (registry_it == registry.end()) {
        throw std::logic_error(
            "atlas::movement::remove_speed_contribution: entity has no base speed seeded via set_base_speed");
    }

    SpeedContributions& speed_contributions = registry_it->second;
    // The removed count is intentionally discarded: removing a source that
    // was never present is a harmless no-op (see this function's own
    // comment in movement.hpp), not something the caller needs to react to.
    static_cast<void>(
        runtime::remove_contributions_by_source<float>(speed_contributions.contributions, source));

    movement_speed->get().base = runtime::resolve_multiplicative<float>(speed_contributions.declared_base,
                                                                        speed_contributions.contributions);
}

RequestResult on_move(Context& ctx, const Move& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    const auto movement_speed = ctx.get<MovementSpeed>(cmd.target);
    if (!movement_speed) {
        return reject(cmd, "target has no MovementSpeed");
    }

    return ctx.get<Position>(cmd.target)
        .transform([&](std::reference_wrapper<Position> position_ref) -> RequestResult {
            Position& position = position_ref.get();

            // Deterministic tick-based distance (spec §4: never wall-clock
            // time inside simulation logic) - cmd.delta_ticks is how many
            // simulation ticks this Move covers, not a sampled duration.
            const float elapsed_seconds =
                static_cast<float>(cmd.delta_ticks) / static_cast<float>(core::Time::ticks_per_second);
            const float distance = movement_speed->get().base * elapsed_seconds;

            position.x += cmd.direction_x * distance;
            position.y += cmd.direction_y * distance;

            ctx.publish<PositionChanged>(PositionChanged{
                .target = cmd.target,
                .new_x = position.x,
                .new_y = position.y,
            });

            return accept(cmd);
        })
        .value_or(reject(cmd, "target has no Position"));
}

} // namespace atlas::movement
