#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/runtime/property_store.hpp"

#include <span>

#include "movement/movement.hpp"

namespace atlas::demo {

// Syncs each tracked entity's render::Transform from its movement::Position
// this tick - the presentation-layer mapping issue #71 needs: demo's own
// gameplay state (2D Position) into atlas-render's composed state
// (Transform's full 3D position/rotation/scale). Never the other direction:
// simulation state is never derived from presentation state (spec §4).
//
// z is always 0 and rotation/scale are left at Transform's own defaults
// (identity rotation, unit scale) - this demo has no notion of entity
// facing/height/non-uniform scale yet (a real gameplay concern, deliberately
// out of this mechanism-proving issue's scope). The point of this function
// is proving the Position -> Transform data flow exists and runs every
// tick, not authoring a complete 3D presentation model.
//
// An entity with no stored Position is silently skipped - an entity simply
// not composing spatial state is an ordinary, expected case (mirrors
// build_frame's own "missing property -> skip" stance), not an error.
void sync_transforms(std::span<const EntityRef> entities,
                     runtime::PropertyStore<movement::Position>& positions,
                     runtime::PropertyStore<render::Transform>& transforms);

} // namespace atlas::demo
