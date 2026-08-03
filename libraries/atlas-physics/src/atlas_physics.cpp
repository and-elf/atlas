// atlas-physics needs at least one non-header-only translation unit for
// CMake to treat it as a regular (non-INTERFACE) library target - CMake
// errors at generate time ("No SOURCES given to target") for a STATIC/
// SHARED library target with zero source files, verified directly against
// this project's own CMake minimum (3.25) rather than assumed. That
// constraint is why this library's CMakeLists.txt always defines
// atlas-physics as a regular library and only conditionally appends
// src/jolt_physics_backend.cpp via target_sources() when
// ATLAS_PHYSICS_BACKEND=JOLT, rather than switching the target's own kind
// (INTERFACE vs STATIC) per backend - see that file's own comment for why a
// single target that changes shape per configure-time option isn't a
// pattern this codebase uses elsewhere (cf. atlas-render's own
// always-regular atlas-render target, which never has this problem because
// it always has real sources - src/frame_builder.cpp etc. - regardless of
// ATLAS_RENDER_BACKEND).
//
// Everything this library provides under the default NULL backend (BodyId,
// BodyCreateInfo/BodyState, the PhysicsBackend concept, NullPhysicsBackend)
// is genuinely header-only with nothing to compile out of line - this file
// exists solely to satisfy the CMake requirement above, not to hold
// implementation. The static_assert below simply re-states
// null_physics_backend.hpp's own (already header-included) contract check
// so this file isn't entirely without content.
#include "atlas/physics/null_physics_backend.hpp"

namespace atlas::physics {

static_assert(PhysicsBackend<NullPhysicsBackend>);

} // namespace atlas::physics
