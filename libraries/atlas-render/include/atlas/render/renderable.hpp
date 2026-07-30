#pragma once

#include "atlas/resource/resource_id.hpp"

namespace atlas::render {

// A renderable entity's resource references - the mesh and material a
// backend draws it with, named by stable identity (spec §3, Resource)
// rather than a hard-coded path or an in-memory pointer. atlas-render
// never opens a mesh/material file or interprets its contents (spec §24:
// content authoring - art direction, shader authoring - stays an
// application concern) - it only carries the identity a future resolver
// (atlas-resource's own README scopes resolution out for now) would
// eventually use to fetch the real asset.
//
// A basic aggregate (rule of zero): no invariant beyond ordinary value
// semantics. A default-constructed Renderable holds two null ResourceIds;
// build_frame (frame_builder.hpp) treats a null id as "does not resolve"
// and skips the entity, exactly as it would any other unresolved
// resource reference.
struct Renderable {
    ResourceId mesh;
    ResourceId material;
};

} // namespace atlas::render
