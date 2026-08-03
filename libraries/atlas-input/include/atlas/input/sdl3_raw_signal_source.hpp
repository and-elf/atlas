#pragma once

#include "atlas/input/raw_signal.hpp"
#include "atlas/windowing/sdl3_shared_window.hpp"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace atlas::input {

// The first real (non-null) atlas::input::RawSignalSource (issue #68):
// keyboard, mouse, and (at most one) game controller, polled via SDL3.
// Satisfies the same RawSignalSource concept NullRawSignalSource/
// ScriptedRawSignalSource do (raw_signal.hpp) - IntentRouter never branches
// on which concrete source it was handed.
//
// **Deliberately reads live state (SDL_GetKeyboardState/SDL_GetMouseState/
// SDL_GetGamepadButton/SDL_GetGamepadAxis), never SDL's event queue
// (SDL_PollEvent).** SDL3's event queue is one shared, per-process queue -
// if this type drained it directly, it could never safely coexist in the
// same process as anything else (a future real atlas::render::Sdl3FrameBackend
// user, a windowing layer) that might also want events, since only one
// reader can ever drain a given queue entry. poll() instead calls
// SDL_PumpEvents() (which updates SDL's internal state buffers and keeps
// the OS message loop alive, without consuming anything another reader
// might also want) and then queries current state directly - multiple
// independent callers doing this is always safe, resolving the "who owns
// the event pump" question raised when this backend was first scoped
// (issue #68) without requiring this library to depend on atlas-render or
// coordinate window ownership with it at all.
//
// **This backend owns its own SDL window** purely to give the OS an input-
// focus target for keyboard/mouse (gamepad state does not need one) - by
// default, via the self-contained constructor below, it does not share a
// window with any real render backend that might exist in the same process.
// A real host running both a real render backend and this input backend
// together and wanting one single visible window instead of two (so real OS
// input focus actually lands where the render backend is presenting) uses
// the alternate constructor below instead, passing an
// atlas::windowing::Sdl3SharedWindow constructed once by the host and also
// handed to atlas::render::Sdl3FrameBackend's own alternate constructor
// (issue #174) - see that type's own doc comment for the full rationale.
// Resolving this is a small neutral library beneath both atlas-render and
// atlas-input, not a dependency from this library onto atlas-render (which
// §13's sibling-library rule forbids either direction of).
//
// **First slice, deliberately scoped:**
// - A curated, fixed table of common gameplay keys/mouse buttons/gamepad
//   buttons/axes (see sdl3_raw_signal_source.cpp) - not exhaustive coverage
//   of every SDL scancode. Extending the table is a low-risk follow-up, not
//   a design gap.
// - At most one connected gamepad is polled (the first one SDL reports) -
//   multiple simultaneous controllers are out of scope.
// - Discrete signals (keys, mouse buttons, gamepad buttons) are only
//   reported while active - matching RawSignalEvent's own "1.0 for a plain
//   discrete signal that is currently active" framing (an inactive one is
//   simply absent from poll()'s result, not present with value 0.0).
//   Continuous signals (mouse position, gamepad stick/trigger axes) are
//   always reported with their current reading, including 0/centered -
//   matching RawSignalEvent's "a continuous reading for an axis" framing.
// - A small fixed dead zone is applied to gamepad stick axes (not
//   configurable this round) so physical stick drift at rest doesn't
//   produce spurious near-zero signal noise.
//
// An encapsulated class, not a basic aggregate (the same exception to Rule
// of Zero CLAUDE.md carves out for atlas::render::Sdl3FrameBackend/
// atlas::entity::EntityRegistry): it owns real OS resources (an SDL_Window,
// an optional SDL_Gamepad handle) with a genuine invariant to protect -
// every acquired handle released exactly once, in the right order.
//
// Construction can fail (SDL video/gamepad subsystem init failure, window
// creation failure) - reported by throwing std::runtime_error rather than
// std::expected, per CLAUDE.md's documented libstdc++/Clang <expected>
// incompatibility.
class Sdl3RawSignalSource {
public:
    // window_title/width/height/extra_window_flags describe the window this
    // backend creates purely as a keyboard/mouse focus target - defaults to
    // a 1x1 hidden window, unlike Sdl3FrameBackend's visible-by-default
    // window, since this backend has nothing to display. A caller wanting a
    // visible window (e.g. manual/interactive testing) can override
    // extra_window_flags.
    explicit Sdl3RawSignalSource(const std::string& window_title = "Atlas",
                                 int width = 1,
                                 int height = 1,
                                 SDL_WindowFlags extra_window_flags = SDL_WINDOW_HIDDEN);

    // Issue #174: the shared-window alternate constructor - borrows
    // shared_window's already-created SDL_Window instead of creating (and
    // later destroying) its own, so real OS keyboard/mouse focus lands on
    // the same single window a real render backend constructed against the
    // same Sdl3SharedWindow is presenting to. shared_window must outlive
    // this instance (the same "must outlive" contract this library's own
    // resource caches document elsewhere in this codebase) - this
    // constructor only initializes SDL's gamepad subsystem for itself
    // (SDL_InitSubSystem(SDL_INIT_GAMEPAD)); video init/window
    // creation/destruction remain entirely shared_window's responsibility.
    //
    // Throws std::runtime_error, with SDL_GetError()'s message included, if
    // SDL gamepad subsystem initialization fails.
    explicit Sdl3RawSignalSource(windowing::Sdl3SharedWindow& shared_window);

    ~Sdl3RawSignalSource();

    // Copying would require duplicating window/gamepad-handle ownership,
    // which SDL has no defined semantics for - deleted, not implemented.
    Sdl3RawSignalSource(const Sdl3RawSignalSource&) = delete;
    Sdl3RawSignalSource& operator=(const Sdl3RawSignalSource&) = delete;

    Sdl3RawSignalSource(Sdl3RawSignalSource&& other) noexcept;
    Sdl3RawSignalSource& operator=(Sdl3RawSignalSource&& other) noexcept;

    // Pumps SDL's internal state (see class doc comment for why this never
    // touches the event queue), then reports every currently-active
    // discrete signal and every continuous signal's current reading from
    // this poll's curated table.
    [[nodiscard]] std::vector<RawSignalEvent> poll();

private:
    void destroy() noexcept;
    void ensure_gamepad_open();

    SDL_Window* window_ = nullptr;
    SDL_Gamepad* gamepad_ = nullptr;
    // True only for an instance whose constructor actually completed (SDL
    // successfully initialized) - false for a default-moved-from instance,
    // matching Sdl3FrameBackend's own owns_sdl_ precedent. Gates whether
    // destroy() does anything at all, regardless of owns_window_ below.
    bool owns_sdl_ = false;
    // True only for an instance constructed via the self-contained
    // constructor above (it created window_ and called
    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) itself) - false for one
    // constructed via the shared-window constructor, where window_ is
    // borrowed and only the gamepad subsystem belongs to this instance.
    // Gates whether destroy() destroys window_ / calls SDL_Quit() versus
    // only SDL_QuitSubSystem(SDL_INIT_GAMEPAD) (see destroy()'s own
    // implementation comment).
    bool owns_window_ = false;
};

static_assert(RawSignalSource<Sdl3RawSignalSource>);

} // namespace atlas::input
