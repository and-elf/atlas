#include "door_hud.hpp"

namespace atlas::door_hud {

namespace {
constexpr atlas::input::IntentId open_door_intent{"OpenDoor"};
} // namespace

atlas::ui::Node build_open_control() {
    return atlas::ui::Node{.clickable = atlas::ui::Clickable{.intent = open_door_intent}};
}

std::optional<atlas::door::OpenDoor> to_open_door_request(const atlas::input::Intent& intent) {
    if (intent.id != open_door_intent) {
        return std::nullopt;
    }
    return atlas::door::OpenDoor{.door = intent.entity};
}

} // namespace atlas::door_hud
