#pragma once

#include "atlas/core/time.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/renderable.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/runtime/property_store.hpp"

#include <span>

namespace atlas::render {

// Builds one Frame (State -> Renderer -> Output, spec §19) from composed
// Transform/Renderable property state, for an explicitly ordered,
// caller-supplied list of entities - never atlas-render's own discovery
// of "which entities exist", which is a capability/host composition
// concern (§5) this library has no business owning. Entities are visited
// in exactly the order given, never re-sorted or grouped: a std::span's
// iteration order is inherently sequential and never derived from an
// unordered_map's own iteration order, so two calls given the same
// entities/store contents always produce a bit-identical Frame,
// satisfying spec §4's determinism guarantee without this function
// needing any ordering logic of its own.
//
// An entity is silently skipped (never substituted with a placeholder,
// never an error) when:
// - it has no stored Transform, or no stored Renderable at all - an
//   entity simply not composing renderable state is an ordinary,
//   expected case, not a bug;
// - its Renderable's mesh or material is the null ResourceId - the
//   closest available analog to "a resource reference that doesn't
//   resolve" this round, since atlas-resource implements identity only
//   and has no resolver yet (see its own README's scoping note): a null
//   id is the one resource state this library can already recognize as
//   definitely not resolving to anything.
//
// tick is never read from a clock inside this function - it is the
// caller's own, already-decided simulation tick (spec §4: no direct
// wall-clock reads feeding rendering's own logic), carried straight
// through to the returned Frame.
[[nodiscard]] Frame build_frame(std::span<const EntityRef> entities,
                                const runtime::PropertyStore<Transform>& transforms,
                                const runtime::PropertyStore<Renderable>& renderables,
                                core::Time tick);

} // namespace atlas::render
