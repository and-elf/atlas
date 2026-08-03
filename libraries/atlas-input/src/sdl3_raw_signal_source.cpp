#include "atlas/input/sdl3_raw_signal_source.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace atlas::input {

namespace {

struct KeyMapping {
    SDL_Scancode scancode;
    std::string_view name;
};

// A curated, first-slice table of common gameplay keys (raw_signal.hpp's
// own doc comment names "KeyE" as the canonical example) - not exhaustive
// coverage of every SDL scancode. Extending this table is a low-risk
// follow-up, never a design gap (see this backend's own class doc comment).
constexpr std::array<KeyMapping, 21> key_table{{
    {SDL_SCANCODE_W, "KeyW"},
    {SDL_SCANCODE_A, "KeyA"},
    {SDL_SCANCODE_S, "KeyS"},
    {SDL_SCANCODE_D, "KeyD"},
    {SDL_SCANCODE_E, "KeyE"},
    {SDL_SCANCODE_Q, "KeyQ"},
    {SDL_SCANCODE_SPACE, "KeySpace"},
    {SDL_SCANCODE_LSHIFT, "KeyLeftShift"},
    {SDL_SCANCODE_LCTRL, "KeyLeftControl"},
    {SDL_SCANCODE_ESCAPE, "KeyEscape"},
    {SDL_SCANCODE_UP, "KeyArrowUp"},
    {SDL_SCANCODE_DOWN, "KeyArrowDown"},
    {SDL_SCANCODE_LEFT, "KeyArrowLeft"},
    {SDL_SCANCODE_RIGHT, "KeyArrowRight"},
    {SDL_SCANCODE_1, "Key1"},
    {SDL_SCANCODE_2, "Key2"},
    {SDL_SCANCODE_3, "Key3"},
    {SDL_SCANCODE_4, "Key4"},
    {SDL_SCANCODE_5, "Key5"},
    {SDL_SCANCODE_TAB, "KeyTab"},
    {SDL_SCANCODE_RETURN, "KeyReturn"},
}};

struct GamepadButtonMapping {
    SDL_GamepadButton button;
    std::string_view name;
};

constexpr std::array<GamepadButtonMapping, 10> gamepad_button_table{{
    {SDL_GAMEPAD_BUTTON_SOUTH, "GamepadButtonSouth"},
    {SDL_GAMEPAD_BUTTON_EAST, "GamepadButtonEast"},
    {SDL_GAMEPAD_BUTTON_WEST, "GamepadButtonWest"},
    {SDL_GAMEPAD_BUTTON_NORTH, "GamepadButtonNorth"},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "GamepadButtonLeftShoulder"},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "GamepadButtonRightShoulder"},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, "GamepadDpadUp"},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, "GamepadDpadDown"},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, "GamepadDpadLeft"},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "GamepadDpadRight"},
}};

struct GamepadAxisMapping {
    SDL_GamepadAxis axis;
    std::string_view name;
    bool is_trigger; // triggers normalize [0, 32767] -> [0, 1]; sticks [-32768, 32767] -> [-1, 1]
};

constexpr std::array<GamepadAxisMapping, 6> gamepad_axis_table{{
    {SDL_GAMEPAD_AXIS_LEFTX, "GamepadLeftStickX", false},
    {SDL_GAMEPAD_AXIS_LEFTY, "GamepadLeftStickY", false},
    {SDL_GAMEPAD_AXIS_RIGHTX, "GamepadRightStickX", false},
    {SDL_GAMEPAD_AXIS_RIGHTY, "GamepadRightStickY", false},
    {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, "GamepadLeftTrigger", true},
    {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, "GamepadRightTrigger", true},
}};

// Physical sticks rarely rest at exactly 0 - a small fixed dead zone (not
// configurable this round, see class doc comment) keeps that drift from
// being reported as spurious near-zero signal noise.
constexpr float stick_dead_zone = 0.1F;

constexpr float normalize_stick(Sint16 raw) {
    const float value = static_cast<float>(raw) / 32768.0F;
    return (value > -stick_dead_zone && value < stick_dead_zone) ? 0.0F : value;
}

constexpr float normalize_trigger(Sint16 raw) {
    return static_cast<float>(raw) / 32767.0F;
}

} // namespace

Sdl3RawSignalSource::Sdl3RawSignalSource(const std::string& window_title,
                                         int width,
                                         int height,
                                         SDL_WindowFlags extra_window_flags) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        throw std::runtime_error(std::string("SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) failed: ") +
                                 SDL_GetError());
    }

    window_ = SDL_CreateWindow(window_title.c_str(), width, height, extra_window_flags);
    if (window_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + error);
    }

    owns_window_ = true;
    owns_sdl_ = true;
    ensure_gamepad_open();
}

// owns_window_ is left at its default-member-initializer value (false) -
// this constructor never owns window_.
Sdl3RawSignalSource::Sdl3RawSignalSource(windowing::Sdl3SharedWindow& shared_window)
    : window_(shared_window.handle()) {
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        throw std::runtime_error(std::string("SDL_InitSubSystem(SDL_INIT_GAMEPAD) failed: ") +
                                 SDL_GetError());
    }

    owns_sdl_ = true;
    ensure_gamepad_open();
}

Sdl3RawSignalSource::~Sdl3RawSignalSource() {
    destroy();
}

Sdl3RawSignalSource::Sdl3RawSignalSource(Sdl3RawSignalSource&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)),
      gamepad_(std::exchange(other.gamepad_, nullptr)),
      owns_sdl_(std::exchange(other.owns_sdl_, false)),
      owns_window_(std::exchange(other.owns_window_, false)) {}

Sdl3RawSignalSource& Sdl3RawSignalSource::operator=(Sdl3RawSignalSource&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::exchange(other.window_, nullptr);
        gamepad_ = std::exchange(other.gamepad_, nullptr);
        owns_sdl_ = std::exchange(other.owns_sdl_, false);
        owns_window_ = std::exchange(other.owns_window_, false);
    }
    return *this;
}

void Sdl3RawSignalSource::ensure_gamepad_open() {
    if (gamepad_ != nullptr && SDL_GamepadConnected(gamepad_)) {
        return;
    }
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }

    int gamepad_count = 0;
    SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&gamepad_count);
    if (gamepad_ids == nullptr) {
        return;
    }
    // At most one connected gamepad (this backend's own first-slice scope,
    // see class doc comment) - the first one SDL reports.
    if (gamepad_count > 0) {
        gamepad_ = SDL_OpenGamepad(gamepad_ids[0]);
    }
    SDL_free(gamepad_ids);
}

std::vector<RawSignalEvent> Sdl3RawSignalSource::poll() {
    SDL_PumpEvents();
    ensure_gamepad_open();

    std::vector<RawSignalEvent> events;

    int key_count = 0;
    const bool* keyboard_state = SDL_GetKeyboardState(&key_count);
    for (const KeyMapping& mapping : key_table) {
        if (mapping.scancode < key_count && keyboard_state[mapping.scancode]) {
            events.push_back(RawSignalEvent{.signal = RawSignalId{mapping.name}, .value = 1.0F});
        }
    }

    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    if ((mouse_buttons & SDL_BUTTON_LMASK) != 0) {
        events.push_back(RawSignalEvent{.signal = RawSignalId{"MouseLeft"}, .value = 1.0F});
    }
    if ((mouse_buttons & SDL_BUTTON_RMASK) != 0) {
        events.push_back(RawSignalEvent{.signal = RawSignalId{"MouseRight"}, .value = 1.0F});
    }
    if ((mouse_buttons & SDL_BUTTON_MMASK) != 0) {
        events.push_back(RawSignalEvent{.signal = RawSignalId{"MouseMiddle"}, .value = 1.0F});
    }
    events.push_back(RawSignalEvent{.signal = RawSignalId{"MouseX"}, .value = mouse_x});
    events.push_back(RawSignalEvent{.signal = RawSignalId{"MouseY"}, .value = mouse_y});

    if (gamepad_ != nullptr) {
        for (const GamepadButtonMapping& mapping : gamepad_button_table) {
            if (SDL_GetGamepadButton(gamepad_, mapping.button)) {
                events.push_back(RawSignalEvent{.signal = RawSignalId{mapping.name}, .value = 1.0F});
            }
        }
        for (const GamepadAxisMapping& mapping : gamepad_axis_table) {
            const Sint16 raw = SDL_GetGamepadAxis(gamepad_, mapping.axis);
            const float value = mapping.is_trigger ? normalize_trigger(raw) : normalize_stick(raw);
            events.push_back(RawSignalEvent{.signal = RawSignalId{mapping.name}, .value = value});
        }
    }

    return events;
}

void Sdl3RawSignalSource::destroy() noexcept {
    if (!owns_sdl_) {
        return;
    }

    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }

    if (owns_window_) {
        // Self-contained construction (this instance created window_ and
        // called SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) itself) -
        // SDL_Quit() tears down every subsystem this instance owns.
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
    } else {
        // Shared-window construction - window_ is borrowed from a
        // windowing::Sdl3SharedWindow this instance does not own, so only
        // the gamepad subsystem this instance itself initialized is torn
        // down; the shared window's own video subsystem init/window/SDL_Quit
        // remain entirely its owner's responsibility.
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        window_ = nullptr;
    }

    owns_sdl_ = false;
    owns_window_ = false;
}

} // namespace atlas::input
