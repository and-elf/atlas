#pragma once

// Generated at build time from demo/modules/door/door.capability.yaml (see
// demo/CMakeLists.txt) - the Door/OpenDoor/DoorOpened contracts.
#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include "door.capability.hpp"

namespace atlas::door {

// The manual implementation of OpenDoor's request handler (spec §14).
// Rejects without authority (§6), rejects if `door` has no seeded Door
// property, and rejects if the door is already open (§6: explicitly reject
// invalid requests rather than silently coercing a no-op into an accept).
// On success, flips `open` in place and publishes DoorOpened carrying the
// door's already-seeded `cue` - the ResourceId naming the sound to play,
// exactly the same way any other composed property is read by whichever
// capability/observer cares, never a bespoke resource-loading message.
[[nodiscard]] RequestResult on_open_door(Context& ctx, const OpenDoor& cmd);

} // namespace atlas::door
