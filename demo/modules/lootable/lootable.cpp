#include "lootable.hpp"

namespace atlas::lootable {

RequestResult on_pick_up(Context& ctx, const PickUp& cmd) {
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    auto item = ctx.get<Lootable>(cmd.item);
    if (!item) {
        return reject(cmd, "item has no Lootable property");
    }

    if (item->get().collected) {
        return reject(cmd, "item already collected");
    }

    item->get().collected = true;

    ctx.publish<PickedUp>(PickedUp{
        .item = cmd.item,
        .collector = cmd.collector,
    });

    return accept(cmd);
}

} // namespace atlas::lootable
