#pragma once

namespace atlas::core {

// A rotation, represented as a quaternion rather than Euler angles - avoids
// gimbal lock and composes predictably under interpolation, the
// conventional choice for both a 3D renderer's transform (atlas-render)
// and a rigid-body simulation's orientation (atlas-physics) - which is
// exactly why this type lives here in atlas-core rather than in either of
// those libraries: atlas-physics must not depend on atlas-render (its own
// README explains why), so the one shared representation both need has to
// sit below both of them. The default value (0, 0, 0, 1) is the identity
// rotation. A basic aggregate (rule of zero): no invariant enforced by
// this type itself - a Quaternion built or interpolated into a non-unit
// length is a caller/precision concern, not something the type constructs
// against.
struct Quaternion {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

} // namespace atlas::core
