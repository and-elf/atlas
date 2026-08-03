# atlas-windowing

The shared SDL3 window/context mechanism (issue #174) that `atlas-render`'s `ATLAS_RENDER_BACKEND=SDL3`
and `atlas-input`'s `ATLAS_INPUT_BACKEND=SDL3` both opt into.

## Why this library exists

`atlas::render::Sdl3FrameBackend` and `atlas::input::Sdl3RawSignalSource` each have their own
self-contained constructor that creates its own `SDL_Init(SDL_INIT_VIDEO)` + `SDL_Window`. That's fine
when a host only wants one of them. It's wrong when a real host wants both a real window *and* real OS
keyboard/mouse input at once: two independent windows means only one of them can ever hold real OS
input focus on a real desktop (X11/Windows/Cocoa), so real key/mouse events would silently never reach
whichever window the OS didn't focus — a hidden or unfocused window never receives them, regardless of
which SDL backend created it.

`atlas::windowing::Sdl3SharedWindow` is the fix: one `SDL_Init(SDL_INIT_VIDEO)` + one `SDL_Window`,
constructed once by whichever host composition wants a real window, then handed by reference to both
`Sdl3FrameBackend` and `Sdl3RawSignalSource`'s own alternate (shared-window) constructors — each still
keeps its original self-contained constructor unchanged, so a host that only ever wants one of
render/input never needs this library at all.

## Why this is its own library, not a dependency between atlas-render and atlas-input

Spec §13's sibling-library rule forbids `atlas-render` and `atlas-input` from depending on each other in
either direction — both are optional capability libraries a headless server host must never gain a
dependency on. A new, small, neutral library sitting *below* both of them (dependency graph: `atlas-core`
→ `atlas-windowing` → `atlas-render`/`atlas-input`) is the only way to let them share a resource without
introducing a sideways dependency spec explicitly rules out.

## Scope

Deliberately SDL3-specific, not a generalized cross-backend "platform window" abstraction — Atlas has
exactly one real windowing backend today (SDL3), and generalizing ahead of a second one actually existing
would be speculative design. Out of scope for this library (see issue #174):

- Implementing the *response* to a window resize (viewport/projection updates on the render side,
  UI relayout) — this library only owns the window's existence, not what any consumer does when its size
  changes.
- Generalizing beyond SDL3.
- `atlas-core` (or any lower library) ever depending on SDL3 — this library sits strictly between
  `atlas-core` and the optional real backends that need it.

## Building it in

`ATLAS_WINDOWING_NEEDS_SDL3` (computed internally from `ATLAS_RENDER_BACKEND`/`ATLAS_INPUT_BACKEND`, not a
cache option of its own) gates whether this library builds a real `Sdl3SharedWindow` (`STATIC`, fetching
SDL3 exactly like `atlas-render`/`atlas-input` already do — `FetchContent_Declare`/`_MakeAvailable` are
idempotent, so all three libraries opting into SDL3 in the same build share one fetch) or is an empty
`INTERFACE` library. Unlike every sibling optional library, there is no meaningful "NULL backend" for this
one to provide: a host that selects neither `ATLAS_RENDER_BACKEND=SDL3` nor `ATLAS_INPUT_BACKEND=SDL3` has
nothing to share a window between. The `atlas::windowing` target always exists either way, so
`libraries/CMakeLists.txt` and every consumer's `CMakeLists.txt` never need to conditionally
`add_subdirectory` or conditionally `target_link_libraries` this library at all.

## Usage

```cpp
#include "atlas/windowing/sdl3_shared_window.hpp"
#include "atlas/render/sdl3_frame_backend.hpp"
#include "atlas/input/sdl3_raw_signal_source.hpp"

atlas::windowing::Sdl3SharedWindow window{"My Game", 1280, 720};
atlas::render::Sdl3FrameBackend frame_backend{registry, window};
atlas::input::Sdl3RawSignalSource input_source{window};
```

Both `frame_backend` and `input_source` now present to / read OS focus from the exact same
`SDL_Window` — `window` must outlive both.
