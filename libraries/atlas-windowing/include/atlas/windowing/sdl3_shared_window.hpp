#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace atlas::windowing {

// The shared SDL3 window/context mechanism issue #174 exists to build:
// atlas::render::Sdl3FrameBackend and atlas::input::Sdl3RawSignalSource each
// independently own their own SDL_Init(SDL_INIT_VIDEO) + SDL_Window when
// constructed via their own self-contained constructors - fine when a host
// only wants one of them, but wrong when a real host wants both a real
// window and real OS keyboard/mouse input at once: two independent windows
// means only one of them can ever hold real OS input focus, so real key/
// mouse events would silently never reach whichever one the OS didn't focus.
// This type is the fix - one SDL_Init(SDL_INIT_VIDEO) + one SDL_Window,
// constructed once by whichever host capability composition wants a real
// window, then handed by reference to both Sdl3FrameBackend and
// Sdl3RawSignalSource's own alternate (shared-window) constructors - see
// each of those types' own doc comments for how they consume it.
//
// Deliberately SDL3-specific, not a generalized cross-backend "platform
// window" abstraction (issue #174's own explicit scope note) - Atlas has
// exactly one real windowing backend today (SDL3), and generalizing ahead of
// a second one actually existing would be speculative design, which
// CLAUDE.md rules out ("Don't ... introduce abstractions beyond what the
// task requires").
//
// A caller that only ever wants one of render or input still uses that
// type's own self-contained constructor directly - this type, and
// atlas-windowing as a whole, is opt-in, never a mandatory extra step.
//
// An encapsulated class, not a basic aggregate (the same Rule-of-Zero
// exception CLAUDE.md carves out for atlas::render::Sdl3FrameBackend/
// atlas::input::Sdl3RawSignalSource): it owns a real OS resource (an
// SDL_Window, plus SDL's own per-process video-subsystem init refcount) with
// a genuine invariant to protect - released exactly once, in the right
// order.
//
// Construction can fail (SDL video subsystem init failure, window creation
// failure) - reported by throwing std::runtime_error rather than
// std::expected, per CLAUDE.md's documented libstdc++/Clang <expected>
// incompatibility.
class Sdl3SharedWindow {
public:
    // title/width/height/extra_window_flags are passed straight through to
    // SDL_CreateWindow, mirroring Sdl3FrameBackend/Sdl3RawSignalSource's own
    // constructor parameters - a caller (e.g. this library's own tests) can
    // opt into SDL_WINDOW_HIDDEN for headless use the same way those types
    // already do.
    //
    // Throws std::runtime_error, with SDL_GetError()'s message included, if
    // SDL video initialization or window creation fails.
    explicit Sdl3SharedWindow(const std::string& title = "Atlas",
                              int width = 1280,
                              int height = 720,
                              SDL_WindowFlags extra_window_flags = 0);

    ~Sdl3SharedWindow();

    // Copying would require duplicating window ownership, which SDL has no
    // defined semantics for - deleted, not implemented.
    Sdl3SharedWindow(const Sdl3SharedWindow&) = delete;
    Sdl3SharedWindow& operator=(const Sdl3SharedWindow&) = delete;

    Sdl3SharedWindow(Sdl3SharedWindow&& other) noexcept;
    Sdl3SharedWindow& operator=(Sdl3SharedWindow&& other) noexcept;

    // The raw SDL_Window handle for a real backend's own alternate
    // (shared-window) constructor to claim for its own device/focus
    // purposes - this type retains ownership (destruction/SDL_Quit) either
    // way, so a caller must never destroy the returned handle itself.
    [[nodiscard]] SDL_Window* handle() const noexcept { return window_; }

private:
    void destroy() noexcept;

    SDL_Window* window_ = nullptr;
    // True only for an instance whose constructor actually completed (SDL
    // successfully initialized) - false for a default-moved-from instance,
    // matching Sdl3FrameBackend/Sdl3RawSignalSource's own owns_sdl_
    // precedent.
    bool owns_sdl_ = false;
};

} // namespace atlas::windowing
