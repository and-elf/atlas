# atlas-input

**Status:** Seeded (issue #28's "minimal TDD scaffold" scope). Implements the `Intent`/binding/router mechanism
end-to-end against an injectable raw input seam — `atlas::input::Intent`/`IntentId`
(`include/atlas/input/intent.hpp`), `atlas::input::RawSignalId`/`RawSignalEvent`/`RawSignalSource`
(`include/atlas/input/raw_signal.hpp`), `atlas::input::InputBinding` (`include/atlas/input/binding.hpp`),
`atlas::input::IntentRouter` (`include/atlas/input/intent_router.hpp`), and
`atlas::input::ScriptedRawSignalSource` (`include/atlas/input/scripted_raw_signal_source.hpp`), the fully
in-memory deterministic test double that proves the mechanism without real hardware. **No real OS input
backend is implemented in this pass** — see "Deferred: real OS backend" below.

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

**`Intent`'s payload is deliberately minimal, not the full §19 shape.** §19 (UI System) notes that atlas-ui's
renderer also produces `Intent` events from UI interaction — same event shape as hardware input, so a click and
a keypress are indistinguishable downstream. atlas-ui is being built in a sibling, independent worktree at the
same time as this library, so this pass does not attempt to match its exact type; `atlas::input::Intent` here
is just `{ IntentId id; float axis = 0.0F; }` — enough to prove binding → routing → consumption, not the fuller
shape spec §5's own example sketches (`entity: EntityRef`, `axis: optional<Vec2>`). **Unifying this `Intent`
with whatever atlas-ui lands on is deferred follow-up work**, to be done once both sides exist and an integrator
can reconcile them deliberately, rather than one guessing at the other's shape now.

**Raw signal events carry no press/release/repeat distinction.** A `RawSignalEvent` is "this signal was observed
this poll, with this value" — not an edge-triggered activation event. A real backend will need edge semantics
(so, e.g., a discrete `Intent` fires once per key press rather than once per tick the key is held); this pass
punts that decision to whoever builds the real backend, since it's meaningless to design without a concrete
backend's actual polling API in hand.

## Deferred: real OS backend

Per issue #28's explicit scope, **no new third-party dependency was added in this pass** — no SDL/GLFW/raw
X11-Wayland-Win32-Cocoa polling. `RawSignalSource` is the seam a follow-up issue plugs a real backend into
(most likely SDL3, given this project's cross-platform Debian/macOS/Windows targets from `CLAUDE.md`); until
then, `ScriptedRawSignalSource` is the only source this library ships.

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

`atlas-input` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard library — no
other Atlas library. Per §5, this library sits below any capability that needs player intention in the
dependency graph, and is optional (§13): a headless server host composes neither `atlas-input` nor `atlas-ui`.

**Provides:** raw platform input polling seam (`RawSignalSource`, `ScriptedRawSignalSource`), binding
configuration (`InputBinding`), `Intent` event production (`IntentRouter`) — the sole source of `Intent` events
entering the capability pipeline; raw key/button/axis data never crosses this boundary.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md#input-as-intent) (Input
as Intent), [§19 UI System](../../docs/specification/19-ui-system.md) (the `Intent` shape atlas-ui's renderer
also produces, noted above as deferred unification work).
