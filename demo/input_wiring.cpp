#include "input_wiring.hpp"

#include <cmath>

namespace atlas::demo {

namespace {

constexpr input::IntentId kMoveForward{"MoveForward"};
constexpr input::IntentId kMoveBackward{"MoveBackward"};
constexpr input::IntentId kMoveLeft{"MoveLeft"};
constexpr input::IntentId kMoveRight{"MoveRight"};

} // namespace

std::vector<input::InputBinding> default_movement_bindings() {
    return {
        input::InputBinding{.raw_signal = input::RawSignalId{"KeyW"}, .intent = kMoveForward},
        input::InputBinding{.raw_signal = input::RawSignalId{"KeyS"}, .intent = kMoveBackward},
        input::InputBinding{.raw_signal = input::RawSignalId{"KeyA"}, .intent = kMoveLeft},
        input::InputBinding{.raw_signal = input::RawSignalId{"KeyD"}, .intent = kMoveRight},
    };
}

std::pair<float, float> resolve_move_direction(std::span<const input::Intent> intents) {
    float direction_x = 0.0F;
    float direction_y = 0.0F;

    for (const input::Intent& intent : intents) {
        if (intent.id == kMoveRight) {
            direction_x += intent.axis;
        } else if (intent.id == kMoveLeft) {
            direction_x -= intent.axis;
        } else if (intent.id == kMoveForward) {
            direction_y += intent.axis;
        } else if (intent.id == kMoveBackward) {
            direction_y -= intent.axis;
        }
    }

    const float length = std::sqrt((direction_x * direction_x) + (direction_y * direction_y));
    if (length <= 0.0F) {
        return {0.0F, 0.0F};
    }
    return {direction_x / length, direction_y / length};
}

} // namespace atlas::demo
