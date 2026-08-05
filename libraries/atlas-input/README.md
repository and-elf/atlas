# atlas-input

**Status:** Seeded (issue #28's "minimal TDD scaffold" scope). Implements the `Intent`/binding/router mechanism
end-to-end against an injectable raw input seam — `atlas::input::Intent`/`IntentId`
(`include/atlas/input/intent.hpp`), `atlas::input::RawSignalId`/`RawSignalEvent`/`RawSignalSource`
(`include/atlas/input/raw_signal.hpp`), `atlas::input::InputBinding` (`include/atlas/input/binding.hpp`),
`atlas::input::IntentRouter` (`include/atlas/input/intent_router.hpp`), and
`atlas::input::ScriptedRawSignalSource` (`include/atlas/input/scripted_raw_signal_source.hpp`), the fully
in-memory deterministic test double that proves the mechanism without real hardware. **A real OS input backend
(`Sdl3RawSignalSource`, issue #68) now also exists** — see "Real OS backend: SDL3" below.

## What this proves (§5, Input as Intent)

Spec §5 requires that raw platform input never crosses into capability code — a capability author only ever
sees a semantic `Intent`, never "was E pressed." This library's job is the boundary itself:

- **`RawSignalId`/`RawSignalEvent`** (`raw_signal.hpp`) — a raw platform signal, named semantically
  (`"KeyE"`, never a device-specific scan code) and carrying a single scalar `value` reading (1.0 for a plain
  discrete signal that's active, a continuous reading for an axis). This pass does not distinguish an
  activation edge from a continuous update — a signal is simply "observed this poll, with this value." That is
  enough to prove the router mechanism; real edge semantics (press/release/repeat) are a backend concern,
  deferred along with the backend itself.
- **`RawSignalSource`** (`raw_signal.hpp`) — a concept, not a virtual interface, for "something that can be
  polled once per tick for the raw signals observed since the last poll." Consistent with how contract
  satisfaction works everywhere else in this codebase (§5, Tiny Interface Composability): a type is a raw input
  source because it has a `poll()` method shaped the right way, not because it inherits from anything. This is
  the seam a future OS backend plugs into.
- **`ScriptedRawSignalSource`** (`scripted_raw_signal_source.hpp`) — the fully in-memory, deterministic test
  double the issue asked for: a scripted queue of raw-signal frames, one consumed per `poll()` call, in order,
  returning an empty frame (never throwing) once exhausted. This is what makes `IntentRouter` testable without
  real hardware, and satisfies §4's determinism constraint that simulation-affecting logic never reads OS/
  hardware entropy directly.
- **`InputBinding`** (`binding.hpp`) — one production rule, raw signal → intent. Binding configuration is data,
  not code (§5): a real binding configuration is just a `std::vector<InputBinding>`, which could equally be
  loaded from a player-editable config file at runtime with no change to `IntentRouter` — this pass builds that
  vector in-memory, since a config-file format is a separate, not-yet-scoped concern (see Open Questions).
- **`IntentRouter`** (`intent_router.hpp`) — the mechanism itself: given a raw-signal stream from an injectable
  `RawSignalSource` and a fixed set of bindings, produces the `Intent` events a capability may observe.
  `poll()`'s return type is `std::vector<Intent>` — nothing raw ever escapes past this call. An unbound raw
  signal is silently ignored (not an error): a signal nothing binds to simply isn't part of the game's intent
  vocabulary right now.
- **`tests/atlas-input/intent_router_test.cpp`'s `CapabilityFacingConsumerOnlyEverObservesIntents`** test is the
  one the issue specifically asked for: a `consume(const std::vector<Intent>&)` function written the way real
  capability code would be can't even name `RawSignalId`/`RawSignalEvent` in its signature — the boundary is
  enforced at compile time, not merely asserted at runtime.

## Scoping decisions

**`Intent` now carries `entity`, matching §5's shape except for `axis`.** §19 (UI System) notes that atlas-ui's
renderer also produces `Intent` events from UI interaction — same event shape as hardware input, so a click and
a keypress are indistinguishable downstream. atlas-ui was originally built in a sibling, independent worktree at
the same time as this library, so an earlier pass of `atlas::input::Intent` stayed at `{ IntentId id; float axis
= 0.0F; }` rather than guess at atlas-ui's not-yet-existing type. Both libraries now exist, and atlas-ui's
`Clickable::invoke()` produces this exact `Intent` type directly (no local stand-in) — see
`libraries/atlas-ui/README.md`'s own Scoping decisions for that side of the unification. `Intent` gained
`entity: EntityRef`, stamped by `IntentRouter::poll`'s caller-supplied polling entity for hardware input, or by
`Clickable`'s own `source` parameter for a UI-produced one. `axis` stays a plain `float` rather than becoming
spec §5's `optional<Vec2>` — no `Vec2` type exists anywhere in this codebase yet (see Open Questions below),
and inventing one wasn't needed to close this particular gap.

**Raw signal events carry no press/release/repeat distinction.** A `RawSignalEvent` is "this signal was observed
this poll, with this value" — not an edge-triggered activation event. A real backend will need edge semantics
(so, e.g., a discrete `Intent` fires once per key press rather than once per tick the key is held); this pass
punts that decision to whoever builds the real backend, since it's meaningless to design without a concrete
backend's actual polling API in hand.

## Real OS backend: SDL3 (issue #68)

`include/atlas/input/null_raw_signal_source.hpp`'s `atlas::input::NullRawSignalSource` is the always-buildable,
zero-third-party-dependency `RawSignalSource` selected by default; `libraries/atlas-input/CMakeLists.txt`'s
`ATLAS_INPUT_BACKEND` CMake option (default `NULL`) picks which concrete source a build compiles in at
configure time, mirroring `atlas-render`'s `ATLAS_RENDER_BACKEND`/`atlas-audio`'s `ATLAS_AUDIO_BACKEND` exactly.
`NullRawSignalSource` is distinct from `ScriptedRawSignalSource`: the latter is a test double requiring a
pre-authored script and is exhausted after replaying it; the former is a genuine zero-configuration production
backend for a build that wants no input source at all.

**`ATLAS_INPUT_BACKEND=SDL3`** builds `Sdl3RawSignalSource` (`include/atlas/input/sdl3_raw_signal_source.hpp`) —
keyboard, mouse, and (at most one) game controller via SDL3's Gamepad API, reusing the exact SDL3 dependency
`atlas-render`'s `ATLAS_RENDER_BACKEND=SDL3` already takes (`FetchContent_Declare`/`_MakeAvailable` are
idempotent, so a build enabling both fetches SDL3 once and both libraries share the same target). SDL3, not a
second cross-platform windowing/input library, was the deliberate choice — see issue #68's own discussion.

**Reads live state (`SDL_GetKeyboardState`/`SDL_GetMouseState`/`SDL_GetGamepadButton`/`SDL_GetGamepadAxis`),
never SDL's event queue (`SDL_PollEvent`) — this is what actually resolves the "who owns the shared SDL event
queue" question issue #68 raised when this backend was first scoped.** SDL3's event queue is one shared,
per-process queue; if this backend drained it directly, it could never safely coexist in the same process as
anything else (a real render backend, a windowing layer) that might also want events. `poll()` instead calls
`SDL_PumpEvents()` (updates SDL's internal state buffers and keeps the OS message loop alive, without consuming
anything another reader might want) and queries current state directly — multiple independent callers doing
this is always safe, without this library depending on `atlas-render` or coordinating window ownership with it
at all.

**This backend owns its own SDL window** by default (the constructor above), purely to give the OS an
input-focus target for keyboard/mouse (gamepad state doesn't need one) — hidden by default (unlike
`Sdl3FrameBackend`'s visible-by-default window, since this backend has nothing to display).
**Answering the "SDL3 for both render and input, or neither" question directly: nothing forces that
coupling.** `ATLAS_RENDER_BACKEND` and `ATLAS_INPUT_BACKEND` are independent CMake options — SDL3 input
alone, SDL3 render alone, both, or neither all build and work; each backend owns whatever OS resources it
needs standalone.

**Issue #174**: a second, alternate constructor, `explicit Sdl3RawSignalSource(atlas::windowing::Sdl3SharedWindow&)`,
borrows an already-created window instead of creating its own — this is the mechanism a real host running
both a real render backend and this input backend together, and wanting one single visible window instead
of two (this backend's own hidden one plus the render backend's own), actually uses. It's a new, small,
neutral library (`atlas-windowing`) sitting below both `atlas-render` and `atlas-input`, not either library
depending on the other — see `libraries/atlas-windowing/README.md` for the full rationale. This constructor
only initializes SDL's gamepad subsystem for itself (`SDL_InitSubSystem(SDL_INIT_GAMEPAD)`); video
init/window creation/destruction remain entirely the shared window's responsibility.

**First slice, deliberately scoped — not exhaustive, not a design gap:**

- A curated, fixed table of common gameplay keys/mouse buttons/gamepad buttons/axes
  (`sdl3_raw_signal_source.cpp`), not every SDL scancode. Extending the table is a low-risk follow-up.
- At most one connected gamepad is polled (the first one SDL reports) — multiple simultaneous controllers are
  out of scope.
- Discrete signals (keys, mouse buttons, gamepad buttons) are only reported while active, matching
  `RawSignalEvent`'s own "1.0 for a plain discrete signal that is currently active" framing; continuous signals
  (mouse position, gamepad stick/trigger axes) are always reported with their current reading, matching its "a
  continuous reading for an axis" framing.
- A small fixed dead zone (not configurable this round) is applied to gamepad stick axes so physical stick
  drift at rest doesn't produce spurious near-zero signal noise.

**A genuine "key/button is currently down" behavior is not testable through SDL3's public API in headless CI.**
`SDL_GetKeyboardState`/mouse-button state is only ever updated by `SDL_SendKeyboardKey`/`SDL_SendMouseButton`,
SDL-internal functions (`src/events/SDL_keyboard.c`/`SDL_mouse.c`) real platform video backends call when
translating genuine OS input — never declared in any public `SDL3/*.h` header. Pushing a synthetic
`SDL_EVENT_KEY_DOWN` via the public `SDL_PushEvent` does not reach them, so it never updates the state arrays
`poll()` actually reads — confirmed by reading SDL3's own fetched source, not assumed. `tests/atlas-input/
sdl3_raw_signal_source_test.cpp` proves the scan loops run cleanly against real SDL3 state arrays every poll
(and, for mouse position specifically, `SDL_WarpMouseGlobal` — a genuinely public API — proves a real value is
read, though the "offscreen" video driver these tests run under has no real display to warp a cursor on, so
even that returns `false` here) and that gamepad absence is handled without crashing, rather than forcing a
synthetic-injection test that would silently test nothing. This is a stated testing limitation, not a defect in
production behavior — a real OS key press does reach `SDL_SendKeyboardKey` through SDL's own video backend.
Construction failure itself (`SDL_Init`/`SDL_CreateWindow`) is tested deterministically via an invalid
`SDL_HINT_VIDEO_DRIVER` value, mirroring `Sdl3FrameBackendConstruction`'s own precedent (issue #151) exactly.

## Open questions for review

- Whether binding configuration should gain a real "load from file" path (§5: "authored as a binding config
  file, player-editable at runtime, no recompile, live rebind") in this library, or whether that belongs to
  `atlas-resource`/`atlas-serialization` instead, with `atlas-input` only ever consuming the resulting
  `std::vector<InputBinding>`. Nothing in this pass depends on a file format, so the decision is still open.
- Whether `RawSignalEvent`/`Intent`'s single `float value`/`axis` field should become a small vector type
  (`Vec2`) once one exists elsewhere in the tree, for two-axis inputs (movement, camera) — no `Vec2` type exists
  in any library yet, so this pass stayed with a single scalar rather than inventing one.
- Whether `IntentRouter` should support many-bindings-per-signal (e.g. the same key producing different intents
  depending on a mode/context), which today is impossible: `InputBinding` is a flat one-to-one mapping and
  `IntentRouter::find_binding` returns the first match. Nothing in issue #28's scope needed this yet.

## Dependency position

`atlas-input` depends publicly on `atlas::entity` (for `atlas::EntityRef`, which `Intent::entity` carries) and
`atlas_project_options`/`atlas_project_warnings`, plus the standard library, in its default
(`ATLAS_INPUT_BACKEND=NULL`) configuration — no other Atlas library. `atlas-ui` depends on `atlas-input` (for
`atlas::input::Intent`/`IntentId`, which `Clickable` produces directly), never the reverse — no cycle. Per §5,
this library sits below any capability that needs player intention in the dependency graph, and is optional
(§13): a headless server host composes neither `atlas-input` nor `atlas-ui`. `SDL3::SDL3-static` is an additional public dependency, but
only when configured with `ATLAS_INPUT_BACKEND=SDL3` — the default build never sees it, matching
`atlas-render`'s own `SDL3::SDL3-static` dependency position exactly. Issue #174 adds `atlas::windowing`
alongside it, same `ATLAS_INPUT_BACKEND=SDL3`-only gating — a downward dependency (`atlas-windowing` sits
below both `atlas-input` and `atlas-render`, per §5/§13), never a sideways one onto `atlas-render` itself.

**Provides:** raw platform input polling seam (`RawSignalSource`, `ScriptedRawSignalSource`), binding
configuration (`InputBinding`), `Intent` event production (`IntentRouter`) — the sole source of `Intent` events
entering the capability pipeline; raw key/button/axis data never crosses this boundary.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md#input-as-intent) (Input
as Intent), [§19 UI System](../../docs/specification/19-ui-system.md) (the `Intent` shape atlas-ui's renderer
also produces, noted above as deferred unification work).
