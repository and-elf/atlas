#include "equipment.hpp"

namespace atlas::equipment {

RequestResult on_equip_armor(Context& ctx, armor::ContributionRegistry& registry, const EquipArmor& cmd) {
    // Request Validation (§6): reject if the request is invalid for
    // current authoritative state.
    if (!ctx.host().has_authority()) {
        return reject(cmd, "not authoritative");
    }

    // "equipment" is a fixed source label, not derived per-item (e.g. from
    // cmd.item), because armor::Contribution::source is a non-owning
    // std::string_view (see property_composition.hpp) - a per-item label
    // built from cmd.item.value would need owned storage this round
    // doesn't add. Nothing yet looks a contribution up by source to remove
    // it selectively (armor.hpp's own scope boundary), so every equipped
    // item sharing one generic label costs nothing today; distinguishing
    // "which specific item to unequip" would be the real trigger to widen
    // Contribution::source to an owned std::string.
    armor::add_contribution(ctx, registry, cmd.target, "equipment", cmd.bonus);

    ctx.publish<ArmorEquipped>(ArmorEquipped{
        .target = cmd.target,
        .item = cmd.item,
        .bonus = cmd.bonus,
    });

    return accept(cmd);
}

} // namespace atlas::equipment
