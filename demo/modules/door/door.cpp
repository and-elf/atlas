#include "door.hpp"

namespace atlas::door {

RequestResult on_open_door(Context& ctx, const OpenDoor& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto door = ctx.get<Door>(cmd.door);
    if (!door) {
        return reject(cmd, "door has no Door property");
    }

    if (door->get().open) {
        return reject(cmd, "door already open");
    }

    door->get().open = true;

    ctx.publish<DoorOpened>(DoorOpened{
        .door = cmd.door,
        .cue = door->get().cue,
    });

    return accept(cmd);
}

} // namespace atlas::door
