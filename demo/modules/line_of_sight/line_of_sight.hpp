#pragma once

// Generated at build time from
// demo/modules/line_of_sight/line_of_sight.capability.yaml (see
// demo/CMakeLists.txt) - the Obstacle contract. No requests/events: this
// capability is properties plus a hand-written query, the same shape
// armor's manifest already establishes (a property-only manifest is a
// legitimate, already-used shape, not a gap - see armor.capability.yaml).
#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"

#include "line_of_sight.capability.hpp"
#include "movement/movement.hpp"

namespace atlas::line_of_sight {

// A named parameter bundle, not three adjacent EntityRef parameters on
// blocks_line_of_sight itself - clang-tidy's
// bugprone-easily-swappable-parameters correctly flags three same-typed
// entity refs in a row as a real hazard (a caller swapping source and
// target, or misplacing obstacle, would silently compile and just answer
// the wrong question). Requiring designated-initializer construction
// (LineOfSightQuery{.obstacle = ..., .source = ..., .target = ...}) turns
// a swap into a compile-time-visible mistake instead of a silent one.
struct LineOfSightQuery {
    EntityRef obstacle;
    EntityRef source;
    EntityRef target;
};

// Returns true if the straight line segment between query.source's and
// query.target's movement::Position passes within query.obstacle's
// declared radius of its center - i.e. the obstacle blocks line of sight
// between them (the standard point-to-segment distance construction:
// project the obstacle's center onto the segment, clamp the projection to
// the segment's own endpoints so the obstacle is never compared against a
// point extrapolated past either end, then compare that closest point's
// distance to radius).
//
// A pure query, not a request: nothing here mutates any property, so
// there's nothing for spec §6's authority validation to apply to - the
// same reason reading armor::Armor's composed value is a direct
// ctx.get<Armor>() call rather than a dispatched request. A future
// spell-casting capability would call this directly from inside its own
// authority-checked request handler, the same way pathing calls into
// movement::on_move rather than reinventing movement's own validation.
//
// Checks exactly one named obstacle, not every Obstacle-bearing entity in
// the world: atlas::runtime::PropertyStore<T> has no iteration in its
// public interface (get/set only), so there's no way to ask "every entity
// with an Obstacle property." A caller checking against several obstacles
// issues one call per obstacle.
//
// Throws std::logic_error if query.obstacle has no Obstacle property, or
// if either query.source or query.target has no movement::Position - a
// setup mistake (an entity that isn't meant to be a line-of-sight
// obstacle, or isn't meant to participate in line-of-sight checks at all,
// should never be passed here), not an ordinary "nothing blocking"
// outcome.
[[nodiscard]] bool blocks_line_of_sight(Context& ctx, const LineOfSightQuery& query);

} // namespace atlas::line_of_sight
