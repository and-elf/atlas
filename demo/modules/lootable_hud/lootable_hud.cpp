#include "lootable_hud.hpp"

namespace atlas::lootable_hud {

namespace {
constexpr atlas::input::IntentId pick_up_intent{"PickUp"};
} // namespace

std::optional<atlas::lootable::PickUp> to_pick_up_request(const atlas::input::Intent& intent,
                                                          atlas::EntityRef collector) {
    if (intent.id != pick_up_intent) {
        return std::nullopt;
    }
    return atlas::lootable::PickUp{.item = intent.entity, .collector = collector};
}

} // namespace atlas::lootable_hud
