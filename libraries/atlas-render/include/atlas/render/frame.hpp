#pragma once

#include "atlas/core/time.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/animation_state.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/resource/resource_id.hpp"

#include <optional>
#include <vector>

namespace atlas::render {

// One resolved draw instruction - the "Output" spec §19's State ->
// Renderer -> Output pattern describes, for a single renderable entity.
// A plain data record a (not-yet-built, deliberately out of scope this
// round - see this library's README) GPU/windowing backend would
// eventually walk to issue real draw calls, carrying everything
// build_frame (frame_builder.hpp) already resolved so nothing downstream
// needs to re-look up PropertyStore state itself. A basic aggregate
// (rule of zero): no invariant beyond ordinary value semantics.
struct DrawCommand {
    EntityRef entity;
    Transform transform;
    ResourceId mesh;
    ResourceId material;

    // nullopt for a non-animated entity - not a skip condition (issue #46).
    // build_frame distinguishes two absence cases that both eventually
    // produce a DrawCommand vs. one that never does: an entity that never
    // composed a CurrentAnimation at all draws normally with this staying
    // nullopt (this field), exactly like before this issue; an entity that
    // did compose one but has no resolved AnimationPose yet is skipped
    // entirely (frame_builder.hpp's own doc comment has the full writeup).
    std::optional<AnimationPose> pose;
};

// The output of one State -> Renderer -> Output pass (spec §19): every
// resolved DrawCommand for one simulation tick, in the deterministic
// order build_frame produced them - Frame itself never re-sorts or
// deduplicates. tick is presentation-only bookkeeping - which simulation
// tick this frame's state was resolved from, useful for a caller
// correlating frames against replay output - Frame never reads it back
// to derive anything else.
//
// A basic aggregate (rule of zero): no invariant beyond ordinary value
// semantics.
struct Frame {
    core::Time tick;
    std::vector<DrawCommand> draw_commands;
};

} // namespace atlas::render
