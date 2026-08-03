#include "physics_observer.hpp"

#include <utility>

namespace atlas::physics_observer {

void observe_body_heights(Context& ctx, const WatchList& watched) {
    for (const EntityRef entity : watched) {
        const auto body_state = ctx.get<rigid_body::BodyState>(entity);
        if (!body_state) {
            continue;
        }

        ctx.set<ObservedBodyState>(entity, ObservedBodyState{.height = body_state->get().position_y});
    }
}

bool schedule_observation_job(runtime::Host& host,
                              const stage::StageId& stage_id,
                              Context& ctx,
                              WatchList watched) {
    return host.schedule(stage_id,
                         [&ctx, watched = std::move(watched)]() { observe_body_heights(ctx, watched); });
}

} // namespace atlas::physics_observer
