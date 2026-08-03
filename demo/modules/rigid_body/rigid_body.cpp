#include "rigid_body.hpp"

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"

#include <algorithm>

namespace atlas::rigid_body {

namespace {

[[nodiscard]] bool has_entry(const BodyRegistry& registry, EntityRef entity) noexcept {
    return std::any_of(
        registry.begin(), registry.end(), [entity](const auto& entry) { return entry.first == entity; });
}

[[nodiscard]] BodyState to_body_state(const physics::BodyState& state) noexcept {
    return BodyState{
        .position_x = state.position.x,
        .position_y = state.position.y,
        .position_z = state.position.z,
        .rotation_x = state.rotation.x,
        .rotation_y = state.rotation.y,
        .rotation_z = state.rotation.z,
        .rotation_w = state.rotation.w,
    };
}

} // namespace

RequestResult
on_spawn_rigid_body(Context& ctx, Backend& backend, BodyRegistry& registry, const SpawnRigidBody& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    if (has_entry(registry, cmd.entity)) {
        return reject(cmd, "entity already has a rigid body");
    }

    const auto position = ctx.get<movement::Position>(cmd.entity);
    if (!position) {
        return reject(cmd, "entity has no Position");
    }

    const physics::BodyCreateInfo create_info{
        .motion_type = cmd.is_dynamic ? physics::BodyMotionType::Dynamic : physics::BodyMotionType::Static,
        .position = core::Vec3{.x = position->get().x, .y = cmd.spawn_height, .z = position->get().y},
        .rotation = core::Quaternion{},
    };

    const physics::BodyId body_id = backend.create_body(create_info);
    registry.emplace_back(cmd.entity, body_id);

    // A just-created body always reports a real BodyState from either
    // backend (physics_backend.hpp's own contract: body_state() only
    // reports std::nullopt for a destroyed or never-created id) - .value()
    // rather than a defensive if/else branch this codebase has no way to
    // exercise the false side of (nothing between create_body() and this
    // call can destroy the body), matching how atlas::Context::get/set
    // itself throws for a genuinely-broken invariant rather than silently
    // tolerating it.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) - see comment above.
    ctx.set<BodyState>(cmd.entity, to_body_state(backend.body_state(body_id).value()));

    return accept(cmd);
}

void advance_rigid_bodies(Context& ctx, Backend& backend, const BodyRegistry& registry, float delta_seconds) {
    // Exactly once per call, regardless of how many bodies registry tracks -
    // never once per entity (see this function's own doc comment,
    // rigid_body.hpp).
    backend.step(delta_seconds);

    for (const auto& [entity, body_id] : registry) {
        const auto state = backend.body_state(body_id);
        if (!state) {
            // Tracked, but the backend body no longer exists (e.g. destroyed
            // directly through the backend) - leave BodyState at whatever it
            // last held rather than erasing or overwriting it with a bogus
            // value.
            continue;
        }

        ctx.set<BodyState>(entity, to_body_state(*state));
    }
}

bool schedule_step_job(runtime::Host& host,
                       const stage::StageId& stage_id,
                       Context& ctx,
                       Backend& backend,
                       const BodyRegistry& registry,
                       float delta_seconds) {
    return host.schedule(stage_id, [&ctx, &backend, &registry, delta_seconds]() {
        advance_rigid_bodies(ctx, backend, registry, delta_seconds);
    });
}

} // namespace atlas::rigid_body
