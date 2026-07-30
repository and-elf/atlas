#include "pathing.hpp"

#include <cmath>
#include <functional>

namespace atlas::pathing {

namespace {

// Below this distance from PathTarget, an entity counts as arrived rather
// than continuing to creep toward it fractionally forever. Not tuned
// against any particular gameplay unit scale - just small enough that
// movement_test.cpp-style whole-unit worked examples arrive cleanly; see
// pathing.hpp / demo/README.md for why exact overshoot/snapping precision
// stays out of scope.
constexpr float k_arrival_epsilon = 0.01F;

} // namespace

RequestResult on_set_path_target(Context& ctx, const SetPathTarget& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    return ctx.get<PathTarget>(cmd.target)
        .transform([&](std::reference_wrapper<PathTarget> path_target_ref) -> RequestResult {
            PathTarget& path_target = path_target_ref.get();
            path_target.has_target = true;
            path_target.target_x = cmd.target_x;
            path_target.target_y = cmd.target_y;
            return accept(cmd);
        })
        .value_or(reject(cmd, "target has no PathTarget"));
}

RequestResult on_advance_pathing(Context& ctx, const AdvancePathing& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    const auto path_target = ctx.get<PathTarget>(cmd.target);
    if (!path_target) {
        return reject(cmd, "target has no PathTarget");
    }

    PathTarget& target_state = path_target->get();
    if (!target_state.has_target) {
        // Idle: no active seek target right now - a legitimate, ordinary
        // steady state (see pathing.hpp for why this mirrors health.cpp's
        // "no Armor" treatment rather than movement.cpp's "no Position"
        // one), not a rejection.
        return accept(cmd);
    }

    const auto position = ctx.get<movement::Position>(cmd.target);
    if (!position) {
        return reject(cmd, "target has no Position");
    }

    const movement::Position& current = position->get();
    const float delta_x = target_state.target_x - current.x;
    const float delta_y = target_state.target_y - current.y;
    const float distance = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));

    if (distance <= k_arrival_epsilon) {
        target_state.has_target = false;
        ctx.publish<PathTargetReached>(PathTargetReached{.target = cmd.target});
        return accept(cmd);
    }

    // Normalized seek direction, handed to movement::on_move exactly the
    // way any other already-semantic movement intent would be (spec §5,
    // Input as Intent) - this is never where movement::Position gets
    // mutated directly, only where a target point becomes a direction for
    // movement's own request handler to act on.
    const float direction_x = delta_x / distance;
    const float direction_y = delta_y / distance;

    return movement::on_move(ctx,
                             movement::Move{
                                 .target = cmd.target,
                                 .direction_x = direction_x,
                                 .direction_y = direction_y,
                                 .delta_ticks = cmd.delta_ticks,
                             });
}

} // namespace atlas::pathing
