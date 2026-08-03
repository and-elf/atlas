#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "input_wiring.hpp"

namespace atlas::demo {
namespace {

TEST(InputWiring, DefaultMovementBindingsMapWasdToTheFourMovementIntents) {
    // "KeyW"/"KeyA"/"KeyS"/"KeyD" match Sdl3RawSignalSource's own real raw
    // signal names exactly (libraries/atlas-input/src/sdl3_raw_signal_source.cpp)
    // so swapping in the real SDL3 backend (issue #71 part 2) needs no
    // binding changes at all.
    const std::vector<input::InputBinding> bindings = default_movement_bindings();

    ASSERT_EQ(bindings.size(), 4U);
    EXPECT_EQ(bindings[0],
              (input::InputBinding{.raw_signal = input::RawSignalId{"KeyW"},
                                   .intent = input::IntentId{"MoveForward"}}));
    EXPECT_EQ(bindings[1],
              (input::InputBinding{.raw_signal = input::RawSignalId{"KeyS"},
                                   .intent = input::IntentId{"MoveBackward"}}));
    EXPECT_EQ(bindings[2],
              (input::InputBinding{.raw_signal = input::RawSignalId{"KeyA"},
                                   .intent = input::IntentId{"MoveLeft"}}));
    EXPECT_EQ(bindings[3],
              (input::InputBinding{.raw_signal = input::RawSignalId{"KeyD"},
                                   .intent = input::IntentId{"MoveRight"}}));
}

TEST(InputWiring, ResolveMoveDirectionReturnsZeroWithNoIntents) {
    EXPECT_EQ(resolve_move_direction({}), (std::pair<float, float>{0.0F, 0.0F}));
}

TEST(InputWiring, ResolveMoveDirectionReturnsAUnitVectorForASingleIntent) {
    const std::vector<input::Intent> intents{{.id = input::IntentId{"MoveForward"}, .axis = 1.0F}};

    const auto [direction_x, direction_y] = resolve_move_direction(intents);

    EXPECT_FLOAT_EQ(direction_x, 0.0F);
    EXPECT_FLOAT_EQ(direction_y, 1.0F);
}

TEST(InputWiring, ResolveMoveDirectionNormalizesDiagonalMovement) {
    const std::vector<input::Intent> intents{
        {.id = input::IntentId{"MoveForward"}, .axis = 1.0F},
        {.id = input::IntentId{"MoveRight"}, .axis = 1.0F},
    };

    const auto [direction_x, direction_y] = resolve_move_direction(intents);

    constexpr float k_inv_sqrt2 = 0.70710678F;
    EXPECT_NEAR(direction_x, k_inv_sqrt2, 1e-5F);
    EXPECT_NEAR(direction_y, k_inv_sqrt2, 1e-5F);
}

TEST(InputWiring, ResolveMoveDirectionCancelsOpposingIntents) {
    const std::vector<input::Intent> intents{
        {.id = input::IntentId{"MoveForward"}, .axis = 1.0F},
        {.id = input::IntentId{"MoveBackward"}, .axis = 1.0F},
        {.id = input::IntentId{"MoveLeft"}, .axis = 1.0F},
        {.id = input::IntentId{"MoveRight"}, .axis = 1.0F},
    };

    EXPECT_EQ(resolve_move_direction(intents), (std::pair<float, float>{0.0F, 0.0F}));
}

TEST(InputWiring, ResolveMoveDirectionIgnoresUnboundIntents) {
    const std::vector<input::Intent> intents{{.id = input::IntentId{"Interact"}, .axis = 1.0F}};

    EXPECT_EQ(resolve_move_direction(intents), (std::pair<float, float>{0.0F, 0.0F}));
}

} // namespace
} // namespace atlas::demo
