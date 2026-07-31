#pragma once

namespace atlas::ui {

// A Node's position, size, and rotation (spec §19, Minimum UI Contract:
// "Node — a positioned, sized element in the UI tree, with a transform and
// optional children"). 2D only, this round: nothing in §19's worked
// examples (AbilitySlot, HealthBar, panel backgrounds) needs a third
// dimension or a full matrix - a flat, screen-space widget tree is the
// scope this pass targets, matching §19's own framing of the UI renderer as
// its own leg of the state -> renderer pipeline, distinct from the 3D
// scene. Widths/heights are independent of a parent's own transform in this
// round - layout composition (how a child's transform relates to its
// parent's) is not defined here; see this library's README.
//
// A basic aggregate (rule of zero): no invariant beyond plain numeric
// fields, so public fields with default member initializers and a
// defaulted comparison, the same shape as atlas::EntityRef/atlas::ResourceId.
struct Transform2D {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    float rotation_degrees = 0.0F;

    friend constexpr bool operator==(const Transform2D&, const Transform2D&) noexcept = default;
};

} // namespace atlas::ui
