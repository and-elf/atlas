#pragma once

namespace atlas::core {

// A 3D position, direction, or scale, in whatever units a composing game's
// own convention uses - atlas-core has no opinion on that (spec §2,
// Mechanism Over Meaning). float, not double: the conventional precision
// this project's consumers of foundational 3D math expect - both a GPU
// vertex pipeline (atlas-render) and a rigid-body simulation
// (atlas-physics) - which is exactly why this type lives here in
// atlas-core rather than in either of those libraries: atlas-physics must
// not depend on atlas-render (its own README explains why), so the one
// shared representation both need has to sit below both of them. A basic
// aggregate (rule of zero): no invariant beyond ordinary value semantics.
struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

} // namespace atlas::core
