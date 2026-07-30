#pragma once

// Generated at build time from demo/modules/pathing/pathing.capability.yaml
// (see demo/CMakeLists.txt) - the PathTarget/SetPathTarget/AdvancePathing/
// PathTargetReached contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "movement/movement.hpp"
#include "pathing.capability.hpp"

namespace atlas::pathing {

// The manual implementation of SetPathTarget's request handler (spec §14).
// Seeds/overwrites entity's single seek target - deliberately one target,
// not a queue: atlas-cgen's manifest type system is scalar-only right now
// (int8..uint64, float, double, bool, EntityRef, ResourceId - see
// tools/atlas-cgen/README.md's "Type mapping"), so a multi-waypoint path
// would need a list/array field type that does not exist yet. Rejects if
// entity has no PathTarget property - like movement's own Position/
// MovementSpeed precedent, an entity issuing SetPathTarget is expected to
// have PathTarget already seeded, so a missing one is a setup mistake, not
// an ordinary outcome.
[[nodiscard]] RequestResult on_set_path_target(Context& ctx, const SetPathTarget& cmd);

// The manual implementation of AdvancePathing's request handler (spec §14).
// Reads entity's current movement::Position and PathTarget, computes the
// normalized direction from one to the other, and internally dispatches
// movement::on_move with that direction and the same delta_ticks (spec §6,
// Terminology: Request vs. Internal Dispatch) - this capability never
// mutates movement::Position directly itself, the same "contribute/dispatch
// through the owning capability's own function, never reach into its state"
// discipline equipment::on_equip_armor already established for armor::Armor.
//
// A PathTarget with has_target == false is treated as a legitimate idle
// steady state, not an error - mirroring health.cpp's treatment of a
// missing armor::Armor (no mitigation, not a rejection) rather than
// movement.cpp's treatment of a missing Position/MovementSpeed (a setup
// mistake): unlike Position/MovementSpeed, PathTarget legitimately toggles
// has_target back to false both before any target is ever set and again on
// arrival, so "nothing to advance toward right now" is an ordinary outcome
// a caller can poll every tick without it ever being a rejection. A missing
// PathTarget property entirely (never seeded) is still rejected, the same
// setup-mistake reasoning as Position/MovementSpeed.
//
// Arrival: once within a fixed epsilon distance of the target, has_target
// is cleared and PathTargetReached is published instead of dispatching a
// final, tiny Move - exact overshoot/snapping precision (stopping exactly
// on the target rather than wherever the last full-speed step lands) is a
// deliberate scope cut, not an oversight; see demo/README.md.
[[nodiscard]] RequestResult on_advance_pathing(Context& ctx, const AdvancePathing& cmd);

} // namespace atlas::pathing
