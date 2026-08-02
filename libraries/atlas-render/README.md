# atlas-render

**Status:** Seeded (issue #30), plus a formal backend contract and null backend (issue #148). Implements the
`State → Renderer → Output` pattern (§19) for 3D rendering: `atlas::render::build_frame`
(`include/atlas/render/frame_builder.hpp`, `src/frame_builder.cpp`) consumes composed `Transform`/`Renderable`
property state — via the real `atlas::runtime::PropertyStore<T>`, not a stub — for an explicitly ordered set of
entities, and produces an `atlas::render::Frame`: an in-memory, testable list of `DrawCommand`s
(`include/atlas/render/frame.hpp`). No *real* GPU/windowing backend exists yet, but the compile-time contract
every backend (real or null) must satisfy now does, along with the always-available `NullFrameBackend` — see
"What's implemented" and Scoping decisions below.

## What's implemented

- **`atlas::render::Vec3`** and **`atlas::render::Quaternion`** (`include/atlas/render/transform.hpp`) —
  plain 3D vector and rotation value types. Basic aggregates (rule of zero): no invariant either type
  enforces itself.
- **`atlas::render::Transform`** (same header) — a renderable entity's full spatial state: `position`,
  `rotation`, `scale` (defaulting to unit scale). A basic aggregate.
- **`atlas::render::lerp`** (three overloads: `float`, `Vec3`, `Transform`) and **`atlas::render::nlerp`**
  (`Quaternion`) — presentation-only interpolation (spec §4: "Wall-clock time may be used only for
  presentation-only concerns (audio/render interpolation)... must never feed back into simulation state").
  `alpha` is always a caller-supplied `double`; none of these functions read a clock of any kind, deterministic
  or otherwise — see Scoping decisions below for why rotation interpolation is `nlerp`, never `slerp`.
- **`atlas::render::Renderable`** (`include/atlas/render/renderable.hpp`) — a renderable entity's resource
  references: `mesh` and `material`, both `atlas::ResourceId` (`atlas-resource`, identity only — see that
  library's own scoping note). A basic aggregate.
- **`atlas::render::DrawCommand`** and **`atlas::render::Frame`** (`include/atlas/render/frame.hpp`) — a
  single resolved draw instruction (`entity`, `transform`, `mesh`, `material`), and the output of one
  render pass: `tick` (`atlas::core::Time`, carried through unmodified — never read back) plus an ordered
  `std::vector<DrawCommand>`. Both basic aggregates.
- **`atlas::render::build_frame`** (`include/atlas/render/frame_builder.hpp`) — the actual State → Renderer
  → Output function. Given a `std::span<const EntityRef>`, a `PropertyStore<Transform>`, a
  `PropertyStore<Renderable>`, and a `Time` tick, it visits entities in exactly the caller-supplied order
  and emits one `DrawCommand` per entity that has both a stored `Transform` and a stored `Renderable` whose
  `mesh` and `material` are both non-null — skipping (never substituting or coercing) every other case.
- **`atlas::render::FrameBackend`** (`include/atlas/render/frame_backend.hpp`, issue #148) — the compile-time
  contract (a C++ `concept`, checked via `static_assert` like every generated contract in this project — spec
  §5: "never a runtime interface table or virtual dispatch lookup") every backend, real or null, must satisfy:
  `submit(const Frame&) -> void` and `last_completed_tick() -> std::optional<core::Time>`. A conforming backend
  must always receive the *complete* `Frame` `build_frame` produced — culling is entirely the backend's own
  concern (issue #117, resolved), specifically so a real backend can do GPU-driven visibility (compute-shader
  culling into an indirect-draw buffer) instead of being forced through CPU-side visibility decisions made
  upstream. `last_completed_tick()` reports the tick of the most recent `Frame` the backend has actually
  *finished* presenting (not merely accepted) — lets a caller do its own backpressure/batching and gives
  frame-drop diagnostics / an FPS meter for free by sampling it over time, without Atlas ever imposing a
  batching policy.
- **`atlas::render::NullFrameBackend`** (`include/atlas/render/null_frame_backend.hpp`, issue #148) — the
  always-buildable `FrameBackend`: does nothing with a `Frame`'s draw commands, zero third-party dependencies.
  Since it performs no real presentation work, the tick it last accepted is "instantly complete" — no GPU
  fence to wait on — so `last_completed_tick()` just reports whatever tick `submit()` was last called with,
  and `std::nullopt` before any call at all.

## Scoping decisions

**Backend selection is a CMake configure-time choice (`ATLAS_RENDER_BACKEND`, default `NULL`), never a runtime
factory or plugin lookup (spec §4).** Only `"NULL"` is implemented today — setting it to anything else fails
the configure step with a clear message rather than silently building nothing. `NullFrameBackend` itself is
header-only and always available regardless of this option; the option instead gates a future *real* backend
(issue #69) being compiled in alongside it. Most CI runners have no real GPU or display hardware, so this keeps
the mechanism up to the backend boundary fully buildable/testable by default, with the real GPU dependency only
pulled in on whichever build actually opts in — see `libraries/atlas-render/CMakeLists.txt`.

**No real GPU/windowing backend this round — deliberately deferred, per issue #30's explicit scope.** No new
third-party dependency (no SDL/Vulkan/bgfx/sokol) was introduced. `build_frame` proves the actual mechanism —
consuming real composed property/resource state and producing a deterministic, testable frame descriptor —
without needing a display, a window, or a GPU context. Wiring `Frame`'s `DrawCommand` list into an actual
rasterizer, a windowing/surface layer, and a real asset resolver (today `atlas::ResourceId` is identity only;
`atlas-resource`'s own README defers resolution) is a distinct, larger follow-up: it needs a third-party
rendering dependency this issue was explicitly scoped not to introduce, and a resolver this repository doesn't
have yet either. Everything in this round is real machinery up to that boundary, not a stand-in for it.

**A null `ResourceId` is this round's answer to "a resource reference that doesn't resolve."** `atlas-resource`
implements identity only, with no resolver yet (see its own README), so there is no real "resolution failure"
this library can observe today. The null id (`ResourceId{}`, `is_null()`) is the one resource state already
meaningful without a resolver — an entity whose `Renderable::mesh` or `::material` is null is treated exactly
like an unresolved reference and skipped, not substituted with a placeholder draw. This is the closest faithful
analog available now; a real resolver's own failure mode (name not found, load error) is a future increment
this function will need to grow into once that resolver exists.

**Rotation interpolation is `nlerp`, never `slerp` — a direct consequence of spec §4, not a stylistic choice.**
`slerp` needs `acos`/`sin`, and unlike the four arithmetic operations and `std::sqrt` (which IEEE-754 requires
to be correctly rounded), the standard does not require transcendental functions to be correctly rounded — two
conforming platforms' `libm` (glibc on Debian 13, macOS's on Apple Silicon, the Windows CRT on x86-64 — exactly
CLAUDE.md's three deployment targets) can legitimately disagree in the last bit or more. `slerp` would have
silently reintroduced cross-platform floating-point divergence in presentation code the moment two machines
interpolated the same rotation. `nlerp` (linear interpolation of the four components, then re-normalize via
`std::sqrt`) avoids every non-required-to-be-correctly-rounded function, at the cost of non-constant angular
velocity along the interpolated path — an acceptable trade for presentation-only rendering, never one this
library would accept for anything feeding back into simulation state.

**`nlerp` falls back to the second rotation rather than dividing by (near) zero.** Two rotations whose
component-wise sum has near-zero length only arises from exactly opposing quaternions (`b == -a`
component-wise) — both represent the same rotation under quaternion double-cover, so returning `b` unmodified
is a safe, deterministic fallback, not a meaningfully different rotation being silently substituted. This is a
real, tested branch (`Nlerp.OppositeQuaternionsFallBackToTheSecondRotationRatherThanDividingByZero`), not
speculative defensiveness.

**`build_frame` takes `std::span<const EntityRef>`, never discovers "which entities exist" itself.** Deciding
what counts as a renderable entity in a given scene is a capability/host-composition concern (§5) this library
has no business owning (spec §2, Mechanism Over Meaning) — `atlas-render` only resolves state for whatever
entities it's handed, in the exact order it's handed them. That ordering guarantee is what makes `build_frame`
itself trivially deterministic (spec §4): a `std::span`'s iteration is inherently sequential, never derived
from an `unordered_map`'s own iteration order, so identical inputs always produce a bit-identical `Frame` —
tested directly (`BuildFrame.MultipleEntitiesAreVisitedInTheCallerSuppliedOrder`,
`BuildFrame.RepeatedCallsWithIdenticalInputProduceIdenticalOutput`).

**`Frame`/`DrawCommand`/`Renderable`/`Transform` deliberately have no comparison operators.** None of them are
used as map/set keys, and every test that needs to compare produced state does so field-by-field (matching this
repo's own precedent, e.g. `atlas-runtime`'s `property_composition_test.cpp`) rather than via a defaulted
`operator==`/`<=>` this library would otherwise need to maintain without an actual caller needing it.

## Open questions (flagging for human review, not silently resolved)

- No real resolver exists yet for `ResourceId` (§13, `atlas-resource`'s own scoping note), so `build_frame`'s
  "null id means unresolved" rule is the best available analog, not the final word: once a resolver exists, a
  *resolvable-but-failed* reference (name not found, asset load error) is a distinct case this function will
  need to grow a real answer for, separate from "the property was simply never set to anything." Whether that
  distinction becomes a new `build_frame` parameter, a richer return type, or a concern the eventual resolver
  itself absorbs is left for whoever designs the first real `atlas-render` backend to decide.
- No actual double-buffered "previous tick" state exists in this round — `lerp`/`nlerp` are pure functions a
  caller can already use today (given two `Transform`s from whatever ticks it's tracking itself), but this
  library doesn't yet own storing "the previous tick's resolved state" itself. Whether that becomes a
  responsibility of a future `atlas-render` frame-history type, or stays the calling host's own concern, is
  left open.
- **No batching/frame-skip policy exists, by design.** `FrameBackend::last_completed_tick()` (issue #148) gives
  a caller the raw signal needed to decide whether to build/submit another `Frame` yet, but `atlas-render`
  deliberately has no opinion on the policy built on top of it (how many ticks behind is "too many," skip vs.
  interpolate) — left entirely to whoever composes the render loop, the same division of responsibility issue
  #117 drew for culling.
- Cross-platform target support (Debian 13 primary, macOS ARM, Windows x86-64, per issue #30's acceptance
  criteria) is confirmed at the *language/standard-library* level this round: everything here is plain C++23
  (`<cmath>`, `<span>`, `<vector>`) with no platform-conditional code, and the `nlerp`-over-`slerp` decision
  above is specifically the floating-point precaution needed for that claim to hold under this project's own
  three-libm reality. It has not been verified by an actual CI build on macOS/Windows in this round — this
  repository's existing CI matrix (`.github/workflows/ci.yml`) already builds/tests release configurations on
  macOS and Windows for every library uniformly, so `atlas-render` inherits that coverage the same way every
  other library here does, rather than needing a bespoke check of its own.

## Provides

3D rendering: consumes composed properties and resources (game state), produces frame output. Serves as one
possible backend for the UI renderer contract (§19), never the mandatory one — backend selection remains a
host composition/deployment concern. `FrameBackend` (issue #148) formalizes that same "never mandatory" stance
one level down: which concrete `FrameBackend` a build compiles in is itself a compile-time choice, with
`NullFrameBackend` always available as the zero-dependency default.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility, now amended to include `atlas-render`), [§19 UI System](../../docs/specification/19-ui-system.md#backend-implementations)
(the State → Renderer → Output pattern this library implements, and the Backend Implementations section
naming it as one possible 3D backend), [§24 Non-Goals](../../docs/specification/24-non-goals.md) (amended:
rendering *technology* is no longer fully out of scope now that this optional reference implementation exists,
but content authoring — art direction, shader authoring — remains an application concern), [§5 Dependency Model](../../docs/specification/05-dependency-model.md)
(dependency rules), [§4 Architectural Invariants](../../docs/specification/04-architectural-invariants.md)
(determinism constraints — the reasoning behind `nlerp` over `slerp`, and behind `tick` never being read from a
clock inside this library), [§20 Property and Resource Composition](../../docs/specification/20-property-and-resource-composition.md)
(the composed properties/resources `build_frame` consumes as input state).

## Dependency position

`atlas-render` depends publicly on `atlas::core` (for `atlas::core::Time` — `Frame::tick`), `atlas::entity`
(for `atlas::EntityRef`), `atlas::resource` (for `atlas::ResourceId`), and `atlas::runtime` (for
`atlas::runtime::PropertyStore<T>`), plus `atlas_project_options`/`atlas_project_warnings` and the standard
library. `atlas::core` is linked directly rather than relied on transitively through `atlas::runtime` —
`atlas-runtime`'s own `CMakeLists.txt` does not link `atlas::core` today (nothing in its current public
interface needs `Time`), so `atlas-render` depends on it explicitly since its own public interface (`Frame`)
does.

Per §5, `atlas-render` is optional: a headless server host never gains a dependency on it, the same as
`atlas-input`, `atlas-ui`, and `atlas-editor`. It sits at the top of the runtime-library stack alongside
`atlas-runtime`, consuming the systems beneath it (entity identity, resource identity, property storage)
rather than being consumed by them — no capability, application, or editor/deployment-specific code is ever a
dependency of this library, consistent with the dependency direction §5 requires throughout.
