# atlas-physics

**Status:** Seeded (issue #177, the first sub-issue of the umbrella #176: rigid-body physics + collision
extension point). Implements the compile-time `PhysicsBackend` concept contract and the always-available
`NullPhysicsBackend` - no real simulation, no third-party dependency, no shapes/collision yet.

**Provides:** rigid-body simulation and collision detection mechanism - a compile-time contract (bodies,
`step()`, queries) plus one reference implementation backend, on the same backend-swappable,
mechanism-not-meaning boundary as `atlas-render`/`atlas-audio` (§24, Non-Goals). Unlike those two, this
library's output feeds back into simulation state and so remains inside the determinism boundary (§4) -
see "Determinism" below.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility; not client-only the way `atlas-input`/`atlas-ui`/`atlas-render` are - a server host composes
it too), [§24 Non-Goals](../../docs/specification/24-non-goals.md) (mechanism-not-meaning: this library
never defines what a body's shape *means* - collider authoring/gameplay semantics stay an application
concern), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (contract satisfaction is a
compile-time fact, never a runtime interface table), [§4 Architectural Invariants](../../docs/specification/04-architectural-invariants.md)
(determinism constraints this library's design follows throughout).

## What's implemented

- **`atlas::physics::BodyId`** (`include/atlas/physics/body_id.hpp`) - a stable, generation-checked handle
  to a body, mirroring `atlas::EntityRef`'s own index+generation pattern exactly: `IndexType`/`GenerationType`
  aliases, a `null_index` sentinel, `is_null()`, defaulted `operator==`. A basic aggregate (rule of zero).
- **`atlas::physics::BodyMotionType`**, **`BodyCreateInfo`**, **`BodyState`** (`include/atlas/physics/body.hpp`)
  - `BodyMotionType` distinguishes `Static` (never moves) from `Dynamic` (simulated) bodies - the minimal
  distinction this round's mechanism-only scope needs. `BodyCreateInfo` is what `create_body()` takes: a
  motion type plus an initial `core::Vec3` position and `core::Quaternion` rotation - deliberately no
  shape/geometry field yet (see "Scoping decisions" below). `BodyState` is what a query returns: the same
  position/rotation pair, resolved for whatever tick it was queried at. Both basic aggregates.
- **`atlas::physics::PhysicsBackend`** (`include/atlas/physics/physics_backend.hpp`) - the compile-time
  contract (a C++ `concept`, checked via `static_assert` like every other backend contract in this project -
  spec §5: "never a runtime interface table or virtual dispatch lookup") every backend, real or null, must
  satisfy: `create_body(const BodyCreateInfo&) -> BodyId`, `destroy_body(BodyId) -> void`,
  `step(float) -> void`, `body_state(BodyId) -> std::optional<BodyState>`. Mirrors `atlas::audio::AudioBackend`
  and `atlas::render::FrameBackend` exactly in shape and intent.
- **`atlas::physics::NullPhysicsBackend`** (`include/atlas/physics/null_physics_backend.hpp`) - the
  always-buildable `PhysicsBackend`: stores each created body's position/rotation in a plain vector and does
  nothing else (no forces, no collision, no shape). See its own doc comment for the exact index-allocation
  and generation-checking choices made (below, "Scoping decisions").
- **`ATLAS_PHYSICS_BACKEND` CMake option** (`libraries/atlas-physics/CMakeLists.txt`, default `NULL`) - which
  concrete backend a build compiles in, resolved at configure time, mirroring `ATLAS_RENDER_BACKEND`/
  `ATLAS_AUDIO_BACKEND` exactly. Only `NULL` exists today; selecting anything else fails the configure step
  with a message pointing at issue #178, the real (Jolt) backend this option will eventually gate.

## Scoping decisions

**No shape/geometry field on `BodyCreateInfo` yet - issue #179's job, not an oversight.** A real physics
engine's body-creation API is defined around what shape primitives it supports (box, sphere, capsule, convex
hull, ...); until #178 brings in a real backend (Jolt) to define that vocabulary against, adding a shape
field here would be pure speculation with nothing but `NullPhysicsBackend` to validate it against - exactly
the kind of undesigned surface this project's architecture principles caution against. This round proves
only that a body can exist, be stepped, and be queried.

**No raycast/sweep query yet - genuinely blocked on #179/#180 needing real shapes to test against, not a
deferred-by-choice gap.** A raycast against a shapeless `NullPhysicsBackend` body has no meaningful answer
to give; the query contract shape is #180's job once there is real geometry to validate it against. This is
explicitly called out as blocked, not simply postponed for convenience.

**`BodyId` mirrors `atlas::EntityRef` exactly, including the index+generation shape**, rather than a plain
opaque integer or pointer - this project already has one proven, tested handle pattern
(`atlas::entity::EntityRegistry`'s own slot/free-list consistency), and a second capability author reading
this handle needs zero new mental model to understand what `generation` is for.

**`NullPhysicsBackend`'s index allocation: monotonically incrementing, never reused, no free-list.** Simplest
correct approach for a backend that exists purely to prove the contract mechanism - a real backend's own
body-handle reuse strategy (e.g. #178/#179's Jolt-backed one) is that backend's own concern, not something
this contract mandates.

**`NullPhysicsBackend::body_state()` does not check `BodyId::generation`.** Because this backend never
reuses an index, every index it ever hands out corresponds to exactly one generation (`0`) for the entirety
of that backend instance's lifetime - the "stale generation" case a check would exist to catch cannot arise
here structurally. A real backend that does reuse indices (and therefore can hand out two different bodies
under the same index at different generations over its lifetime) must check generation itself; this
contract does not mandate the check because it cannot mandate a specific reuse policy either.

**`step()`'s `delta_seconds` is a plain caller-supplied fixed timestep, never internally sourced from a
clock** - mirrors `atlas::render::lerp(Transform...)`'s own `alpha`-is-caller-supplied discipline, and is a
direct, load-bearing consequence of spec §4's determinism rule (see "Determinism" below), not a style
preference.

## Determinism

Unlike `atlas-render`/`atlas-audio` (presentation-only, explicitly excluded from the determinism boundary -
§4), `atlas-physics`'s output feeds back into simulation state: a rigid body's resolved position/rotation is
exactly the kind of state spec §4's bit-exact determinism guarantee covers. This round's `NullPhysicsBackend`
trivially satisfies that guarantee (it does no computation at all - `step()` is a genuine no-op), but the
contract itself is written with a real, determinism-bound backend in mind: `step()` never sources its
timestep from a clock, and nothing in `PhysicsBackend`'s shape gives a conforming backend room to read
wall-clock time or unseeded entropy. The concrete determinism-preserving build flags and coding practices a
real backend (Jolt, #178) will need (e.g. Jolt's own `CROSS_PLATFORM_DETERMINISTIC` option) are that
backend's own concern to document, per issue #176's own reasoning - not baked into this contract.

## Dependency position

`atlas-physics` depends publicly on `atlas::core` only (for `core::Vec3`/`core::Quaternion`), plus
`atlas_project_options` - following the `atlas-core` CMake pattern, adapted for a header-only interface
library (`add_library(atlas-physics INTERFACE)`, since this round's entire contents - the concept plus
`NullPhysicsBackend` - are header-only). Per §13, this library is optional in the ordinary sense any
capability library is (a game that doesn't need physics doesn't compose it), but it is **not** client-only
the way `atlas-input`/`atlas-ui`/`atlas-render` are: a server host authoritatively simulating physics-affected
state composes `atlas-physics` exactly as a client does. `atlas-physics` deliberately does **not** depend on
`atlas-render` - even though both libraries need the exact same `Vec3`/`Quaternion` math types, that shared
need is exactly why those types were relocated to `atlas-core` (this issue's own Part A) rather than
`atlas-physics` picking up a dependency on `atlas-render` to reuse them, which would have wrongly made a
headless server's physics simulation depend on a presentation-only library.

## Open questions (flagging for human review, not silently resolved)

- **Real rigid-body simulation** - `step()` currently does nothing; #178 (Jolt bring-up) and #179 (real
  bodies/shapes wired to the contract) are the sub-issues that give it actual behavior.
- **Raycast/sweep query API** - #180's job, once #179 gives the contract real shapes to query against.
- **`Camera`/collision wiring** - #181/#182, downstream of both this contract and `atlas-render`'s own
  eventual `Camera` type; independent of this round entirely.
- **CI wiring** - #183, mirroring #161's rigor bar for `atlas-render`'s own CI integration; deliberately not
  touched by this issue per its own explicit scope boundary.
- **Body-handle reuse policy for a real backend** - left to #178/#179 to decide and document, per
  `NullPhysicsBackend`'s own scoping note above.

## References

- #176 (parent/umbrella: rigid-body physics + collision extension point)
- #177 (this issue: `PhysicsBackend` concept contract + `NullPhysicsBackend`)
