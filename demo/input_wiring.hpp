#pragma once

#include "atlas/input/binding.hpp"
#include "atlas/input/intent.hpp"

#include <span>
#include <utility>
#include <vector>

namespace atlas::demo {

// This demo's own binding configuration (issue #71 part 1): WASD -> the
// four movement intents PresentationApp resolves into a Move request each
// tick. Matches Sdl3RawSignalSource's own real "KeyW"/"KeyA"/"KeyS"/"KeyD"
// raw signal names (libraries/atlas-input/src/sdl3_raw_signal_source.cpp)
// exactly, so swapping in the real SDL3 backend (a separate follow-up issue)
// needs no binding changes at all - only the RawSignalSource this demo polls
// changes.
[[nodiscard]] std::vector<input::InputBinding> default_movement_bindings();

// Aggregates one poll's worth of resolved movement intents into a single,
// already-normalized (direction_x, direction_y) pair - {0, 0} if no movement
// intent was active. movement::on_move's own doc comment expects
// direction_x/direction_y "pre-normalized by the caller" (spec §5, Input as
// Intent: a capability handler is never where raw axes become a unit
// vector) - this is that normalization step. It lives here in demo/, not
// atlas-input, since which four intents compose into a 2D direction is this
// demo's own choice, not a general input-library concern.
[[nodiscard]] std::pair<float, float> resolve_move_direction(std::span<const input::Intent> intents);

} // namespace atlas::demo
