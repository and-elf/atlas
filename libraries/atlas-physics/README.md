# atlas-physics

**Status:** Real rigid-body simulation wired to the contract (issue #179, the third sub-issue of the
umbrella #176: rigid-body physics + collision extension point). Issue #177 implemented the compile-time
`PhysicsBackend` concept contract and the always-available `NullPhysicsBackend`; #178 added
`JoltPhysicsBackend`, the first backend that genuinely simulates, behind one hardcoded placeholder
collision shape; #179 replaces that placeholder with a real, backend-agnostic shape vocabulary
(box/sphere/capsule/convex hull), proves genuine collision resolution (a `Dynamic` body settling on a
`Static` floor rather than falling through it), and adds this library's first bit-exact determinism test.

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
- **`atlas::physics::BodyMotionType`**, **`BodyShape`**, **`BodyCreateInfo`**, **`BodyState`**
  (`include/atlas/physics/body.hpp`) - `BodyMotionType` distinguishes `Static` (never moves) from `Dynamic`
  (simulated) bodies. `BodyShape` (issue #179) is a `std::variant<BoxShape, SphereShape, CapsuleShape,
  ConvexHullShape>` - a plain, backend-agnostic shape vocabulary limited to exactly the primitive kinds Jolt
  supports natively (see "Shape vocabulary (issue #179)" below for the full design and its resolution of the
  issue's own ambiguous wording). `BodyCreateInfo` is what `create_body()` takes: a motion type, an initial
  `core::Vec3` position, a `core::Quaternion` rotation, and a `shape` (defaulting to a 0.5m-radius
  `SphereShape` - #178's own hardcoded placeholder radius, so every pre-#179 call site keeps compiling and
  behaving unchanged). `BodyState` is what a query returns: the same position/rotation pair (still no shape -
  a body's own shape never changes after creation in this round's scope, so there is nothing for a query to
  report that create_body() didn't already fix), resolved for whatever tick it was queried at. All basic
  aggregates.
- **`atlas::physics::PhysicsBackend`** (`include/atlas/physics/physics_backend.hpp`) - the compile-time
  contract (a C++ `concept`, checked via `static_assert` like every other backend contract in this project -
  spec §5: "never a runtime interface table or virtual dispatch lookup") every backend, real or null, must
  satisfy: `create_body(const BodyCreateInfo&) -> BodyId`, `destroy_body(BodyId) -> void`,
  `step(float) -> void`, `body_state(BodyId) -> std::optional<BodyState>`. Mirrors `atlas::audio::AudioBackend`
  and `atlas::render::FrameBackend` exactly in shape and intent.
- **`atlas::physics::NullPhysicsBackend`** (`include/atlas/physics/null_physics_backend.hpp`) - the
  always-buildable `PhysicsBackend`: stores each created body's position/rotation in a plain vector and does
  nothing else (no forces, no collision, no shape simulation). Accepts `BodyCreateInfo::shape` (issue #179)
  exactly like every other field it doesn't act on. See its own doc comment for the exact index-allocation
  and generation-checking choices made (below, "Scoping decisions").
- **`atlas::physics::JoltPhysicsBackend`** (`include/atlas/physics/jolt_physics_backend.hpp`,
  `src/jolt_physics_backend.cpp`) - the first real `PhysicsBackend`: a genuine `JPH::PhysicsSystem`, stepped
  by a real fixed timestep, with every body backed by an actual Jolt `JPH::BodyID` rather than a
  locally-echoed pose. `create_body()` converts `BodyCreateInfo::shape` into a real `JPH::BoxShape`/
  `JPH::SphereShape`/`JPH::CapsuleShape`/`JPH::ConvexHullShape` (issue #179, replacing #178's own hardcoded
  placeholder sphere - see "Shape vocabulary (issue #179)" below). Fully real from `create_body()` through
  `step()` through `body_state()`, including genuine collision resolution between bodies (`tests/atlas-physics/
  jolt_physics_backend_test.cpp`'s `DynamicBodySettlesOnStaticFloorAndDoesNotFallThrough`). Only compiled when
  `ATLAS_PHYSICS_BACKEND=JOLT`.
- **`ATLAS_PHYSICS_BACKEND` CMake option** (`libraries/atlas-physics/CMakeLists.txt`, default `NULL`) - which
  concrete backend a build compiles in, resolved at configure time, mirroring `ATLAS_RENDER_BACKEND`/
  `ATLAS_AUDIO_BACKEND` exactly. `NULL` (default) and `JOLT` (FetchContent's real Jolt Physics) both exist
  today; selecting anything else fails the configure step with a clear message.

## Shape vocabulary (issue #179)

Issue #179's own text says shapes should use "shape primitives Jolt supports natively (box, sphere, capsule,
convex hull) rather than inventing Atlas's own shape representation" - taken completely literally this would
mean exposing Jolt's own C++ shape types (`JPH::Shape` et al.) directly through `BodyCreateInfo`, but that is
inconsistent with this library's own established architecture: `body.hpp` is the **contract layer**, shared
by `NullPhysicsBackend` (zero third-party dependencies, mirroring `NullFrameBackend`/`NullAudioBackend`'s own
"compiles everywhere" role) and `JoltPhysicsBackend` alike - it cannot depend on Jolt's own headers without
breaking that.

**Resolution:** limit the vocabulary to the same small set of primitive kinds Jolt supports natively
(box/sphere/capsule/convex hull) rather than inventing something exotic (a custom mesh-collider format, novel
primitive types), but express them as a plain, backend-agnostic Atlas-defined data type in `body.hpp`:

```cpp
struct BoxShape { core::Vec3 half_extents = {.x = 0.5F, .y = 0.5F, .z = 0.5F}; };
struct SphereShape { float radius = 0.5F; };
struct CapsuleShape { float half_height = 0.5F; float radius = 0.5F; };
struct ConvexHullShape { std::vector<core::Vec3> points; };
using BodyShape = std::variant<BoxShape, SphereShape, CapsuleShape, ConvexHullShape>;
```

`JoltPhysicsBackend::create_body()` converts a `BodyShape` into a real `JPH::BoxShape`/`JPH::SphereShape`/
`JPH::CapsuleShape`/`JPH::ConvexHullShape` via `std::visit` (`make_jolt_shape()`,
`src/jolt_physics_backend.cpp`); `NullPhysicsBackend` simply stores/ignores it exactly the way it already
ignores everything else about a body's state beyond position/rotation. Each alternative is a basic aggregate
(rule of zero) - no invariant enforced by the type itself.

**`JPH::ConvexHullShape` needed real investigation, not assumption** - unlike `JPH::BoxShape`/
`JPH::SphereShape`/`JPH::CapsuleShape` (each a plain-data `explicit` constructor: half-extent/radius/
half-height+radius respectively), `JPH::ConvexHullShape` has no direct plain-data constructor (verified
against the real fetched Jolt source, `Jolt/Physics/Collision/Shape/ConvexHullShape.h`). Building a hull can
fail (too few points, or points too degenerate/collinear to form one), so Jolt's own API is
settings-then-`Create()`: build a `JPH::ConvexHullShapeSettings` from the raw point list, call `Create()`,
and check the returned `JPH::Shape::ShapeResult` for an error before trusting it holds a shape.
`make_jolt_shape()` surfaces a hull-construction failure by throwing `std::runtime_error`, the same
convention `create_body()` already used for its own body-budget-exhausted failure (issue #178) - exercised by
`JoltPhysicsBackend.CreateBodyWithDegenerateConvexHullShapeThrows` (an empty point list).

**No raycast/sweep query yet - genuinely blocked on #180 needing real shapes to test against, not a
deferred-by-choice gap.** A raycast against a shapeless `NullPhysicsBackend` body has no meaningful answer
to give; the query contract shape is #180's job now that #179 gives the contract real geometry to validate
against. This is explicitly called out as blocked, not simply postponed for convenience.

## Scoping decisions

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

## Scoping decisions (Jolt)

**Every body's real shape (issue #179) - no more hardcoded placeholder.** #178's own every-body-gets-a-
0.5m-radius-`JPH::SphereShape` placeholder is gone: `create_body()` now converts `create_info.shape`
(`BodyCreateInfo`'s own `BodyShape` variant) into the matching real Jolt shape via `make_jolt_shape()` (see
"Shape vocabulary (issue #179)" above for the full design). `JoltPhysicsBackend` remains fully real
end-to-end: `body_state()` reports a genuinely Jolt-integrated pose, queried live from `JPH::BodyInterface`,
never a cached echo - this is architecturally simpler than tracking two parallel notions of body state (a
"real" Jolt body plus a locally-cached one) and gives a natural, unambiguous way to prove the mechanism is
real: a `Dynamic` body's queried Y position measurably falls under `step()`, a `Static` body's does not, and
(issue #179) a `Dynamic` body genuinely collides with and comes to rest on a `Static` body's real shape
rather than falling through it (`tests/atlas-physics/jolt_physics_backend_test.cpp`).

**Body-handle reuse policy: monotonically incrementing index, never reused - the same choice
`NullPhysicsBackend` made, for the same reason.** `JoltPhysicsBackend` stores its own
`BodyId::index -> JPH::BodyID` map as a `std::vector<std::optional<JPH::BodyID>>`, exactly mirroring
`NullPhysicsBackend`'s own `bodies` vector shape; `destroy_body()` clears a slot to `std::nullopt` but never
shrinks the vector or reuses an index. Every `BodyId` this backend ever hands out therefore has generation
`0` for its entire lifetime, so `body_state()`/`destroy_body()` never need to check `BodyId::generation`, for
the identical structural reason `NullPhysicsBackend` doesn't (see #177's own scoping note - it applies
unchanged here). A reuse policy that recycles a destroyed body's slot (bumping generation, the way
`atlas::entity::EntityRegistry` does) remains a possible future refinement, not something this issue's scope
requires.

**Jolt's own process-wide global registration (`JPH::RegisterDefaultAllocator()`, `JPH::Factory::sInstance`,
`JPH::RegisterTypes()`) is initialized exactly once per process and deliberately never torn down.** Jolt
provides no reference count of its own around this registration - `JPH::RegisterTypes()` populates a global
collision-dispatch table (`JPH::CollisionDispatch::sInit()` plus each shape type's own `sRegister()`) that
every live `JoltPhysicsBackend` instance in the process depends on. A function-local static guard
(`ensure_global_jolt_init()`, `src/jolt_physics_backend.cpp`) makes the registration idempotent across
however many instances get constructed (this library's own test binary constructs a fresh
`JoltPhysicsBackend` per `TEST()`), and no instance's destructor ever calls `JPH::UnregisterTypes()` /
deletes `JPH::Factory::sInstance` - doing so while another instance might still be alive and simulating would
silently break that other instance's collision detection. The resulting one-time, bounded cost (a single
`JPH::Factory`, kept alive for the process's entire lifetime) is a deliberate, documented trade-off, verified
against Jolt's own `Jolt/RegisterTypes.cpp` rather than assumed - this mirrors how Jolt's own single-process
examples (HelloWorld, the Samples application) also register exactly once, at process startup, and only ever
unregister once, at process exit, never per simulated object's own lifetime.

**This global-init guard must run before any other member of `JoltPhysicsBackend` is constructed - a real,
reproduced bug during this issue's own development, not a hypothetical ordering concern.** The first working
version of this class called `ensure_global_jolt_init()` from inside the constructor's own body, after its
member-initializer list - which crashed with a null-function-pointer `SIGSEGV` inside
`JPH::TempAllocatorImpl::TempAllocatorImpl` (reproduced and diagnosed under `gdb`): `JPH::TempAllocatorImpl`'s
constructor calls straight through Jolt's global `JPH::Allocate`/`JPH::AlignedAllocate` function pointers,
which are still null until `JPH::RegisterDefaultAllocator()` runs - and C++ member initialization always
happens in declaration order, strictly before a constructor's own body executes, regardless of what that body
or the member-initializer list itself says. The fix: a dedicated, empty `GlobalJoltInit` member
(`include/atlas/physics/jolt_physics_backend.hpp`) whose own constructor calls
`ensure_global_jolt_init()`, declared as the *first* member of `JoltPhysicsBackend` so it is unconditionally
constructed before `temp_allocator_` or anything else - see that member's own doc comment for the full
explanation, kept next to the field itself so a future member reordering doesn't silently reintroduce this
bug.

**Construction: investigated, and found to realistically never fail - unlike `Sdl3FrameBackend`.**
`Sdl3FrameBackend`'s constructor fails whenever this machine has no GPU/display (the common case on a
headless CI runner) - CPU-side physics has no equivalent hardware dependency. `JPH::PhysicsSystem::Init()`
itself returns `void` with no documented failure path, and neither `JPH::RegisterDefaultAllocator()` nor
`JPH::RegisterTypes()` (absent a Jolt-version mismatch across translation units, which would be a build
misconfiguration, not a runtime condition, and which Jolt reports via `std::abort()`, not an exception,
making it uncatchable regardless) has one either. `JoltPhysicsBackend`'s constructor is therefore documented
as non-throwing, and this library deliberately has no "construction failure" test the way
`Sdl3FrameBackend`'s own test suite does - a hollow test asserting "doesn't throw" would add nothing real.
`create_body()` is a different story: `JPH::BodyInterface::CreateAndAddBody` genuinely can fail (an invalid
`JPH::BodyID`) once this instance's fixed body budget (`max_bodies`, 1024, matching JoltPhysicsHelloWorld's
own example value) is exhausted - that real failure is reported by throwing `std::runtime_error`, per this
project's established `std::expected`-incompatibility convention, and is exercised by
`CreateBodyThrowsOnceThisInstancesBodyBudgetIsExhausted`.

**Threading: `JPH::JobSystemSingleThreaded`, not `JPH::JobSystemThreadPool`.** This is the first real Jolt
bring-up, and "prove the mechanism, not production-ready" is this project's own established bar elsewhere
(`atlas-render`'s README: HLSL compiled at runtime rather than offline). A single-threaded job system is the
simplest correct choice to reason about and verify deterministic - Jolt's own documentation
(`Docs/Architecture.md`, "Deterministic Simulation") notes that several callback/query orderings
(`BodyActivationListener`, `ContactListener`, `PhysicsSystem::GetActiveBodies`) are explicitly **not**
deterministic when multiple threads are involved, precisely because Jolt's own job system runs them from
multiple threads; running single-threaded sidesteps that whole class of concern for this round rather than
needing to prove none of this backend's own code depends on any of those orderings. Multithreaded job
scheduling as a performance optimization is explicitly a future concern (issue #176's own umbrella
breakdown), not this issue's job.

## Jolt CMake integration (issue #178)

`libraries/atlas-physics/CMakeLists.txt`'s `ATLAS_PHYSICS_BACKEND=JOLT` branch `FetchContent`s Jolt Physics
v5.6.0 (the latest stable tag at the time of this issue, confirmed via `git ls-remote --tags` against the
real upstream repository) from `SOURCE_SUBDIR "Build"` (Jolt's own top-level `CMakeLists.txt` lives at
`Build/CMakeLists.txt`, not the repository root - verified directly against the fetched checkout). The
starting point was JoltPhysicsHelloWorld's own official reference `FetchContent` block, adapted - not copied
verbatim - after investigating each option below against Jolt's own `Build/CMakeLists.txt` and
`Jolt/Jolt.cmake` (v5.6.0):

- **`CROSS_PLATFORM_DETERMINISTIC ON`, not `OFF`** (the HelloWorld reference's own value) - non-negotiable
  given spec §4 and `docs/specification/24-non-goals.md`'s amendment that `atlas-physics` stays inside the
  determinism boundary, unlike `atlas-render`/`atlas-audio`. Jolt's own `Docs/Architecture.md`
  ("Deterministic Simulation") documents this as the supported way to get bit-exact results across compilers/
  OSes/architectures/word sizes at a real, measured ~8% performance cost, and confirms it also disables FMA
  contraction (`Jolt/Jolt.cmake`: "FMA is not compatible with cross platform determinism") - exactly this
  project's own `-ffp-contract=off` requirement (root `CMakeLists.txt`), applied to Jolt's own code as well.
  Verified (not assumed) that this project's own sanitized debug preset still configures, builds, and links
  cleanly with this option `ON` - see "Verification" below.
- **`USE_SSE4_1`/`USE_SSE4_2`/`USE_AVX`/`USE_AVX2 ON`, `USE_AVX512 OFF`** - matching the HelloWorld reference
  unchanged, even though these are x86-specific SIMD features and this project also targets macOS/ARM64
  (CLAUDE.md). Verified directly against `Jolt/Jolt.cmake`: the actual compiler flags (`-msse4.1`, `-mavx2`,
  etc.) and the corresponding `JPH_USE_*` preprocessor defines are only ever applied inside a branch guarded
  by `CMAKE_SYSTEM_PROCESSOR`/`CMAKE_OSX_ARCHITECTURES`/`CROSS_COMPILE_ARM` ("ARM64 uses no special
  commandline flags", `Jolt/Jolt.cmake`) - so these cache variables are already silently inert on an ARM64
  configure and need no additional guarding on this project's side. **Not independently re-verified in this
  sandbox** (x86-64 only) - logically correct per Jolt's own CMake source, but the ARM64 configure path is
  flagged here as untested-in-this-environment rather than silently assumed to work.
- **`OVERRIDE_CXX_FLAGS ON`**, matching the reference - verified this does **not** leak into this project's
  own compiler flags. `Build/CMakeLists.txt` sets `CMAKE_CXX_FLAGS`/`CMAKE_CXX_FLAGS_DEBUG`/
  `CMAKE_CXX_FLAGS_RELEASE` via plain (non-`CACHE`, non-`PARENT_SCOPE`) `set()` calls, which under CMake's own
  directory-variable scoping rules only affects the directory `FetchContent` adds Jolt's `Build/CMakeLists.txt`
  as (and any of Jolt's own sub-targets added within that same scope - none are, since
  `TARGET_UNIT_TESTS`/`TARGET_HELLO_WORLD`/etc. are explicitly off), never propagating up to this project's own
  directory scope where `atlas_project_options`/`atlas_project_warnings`/`atlas-physics` are defined.
  Confirmed empirically that `atlas_project_warnings`'s own `-Werror` and this project's global
  `-ffp-contract=off` both still apply, unaffected, to every `atlas::` target after this `FetchContent` block
  runs.
- **`CPP_EXCEPTIONS_ENABLED OFF`**, matching the reference - a per-translation-unit compilation choice for
  Jolt's own sources, not a whole-program ABI switch. `JoltPhysicsBackend`'s own code (an ordinary
  atlas-physics translation unit, compiled with exceptions enabled as normal) never needs an exception to
  unwind through a Jolt-compiled frame: it only calls into Jolt's public API from outside, and that API
  reports failure via return values (e.g. an invalid `JPH::BodyID`), never a thrown exception. No link or ABI
  issue was observed building and running this library's own tests.
- **`CPP_RTTI_ENABLED ON`, not the HelloWorld reference's own `OFF`** - discovered empirically, not assumed.
  With `CPP_RTTI_ENABLED OFF` (Jolt's own default), linking this library's own `jolt_physics_backend.cpp`
  (compiled with this project's own ambient RTTI-enabled default - nothing in this project passes
  `-fno-rtti` anywhere) against Jolt's own compiled objects failed with a real, reproduced link error:
  `undefined reference to typeinfo for JPH::Shape/JPH::ConvexShape/JPH::SphereShape/
  JPH::JobSystemSingleThreaded/...`. GCC only emits a polymorphic class's `typeinfo` object from the
  translation unit defining its virtual "key function" when *that* translation unit is itself compiled with
  RTTI enabled; Jolt's own `.cpp` files (compiled `-fno-rtti` by default) never emit it, leaving this
  project's own RTTI-enabled code - which touches these polymorphic Jolt types directly (`new
  JPH::SphereShape(...)`, a `JPH::JobSystemSingleThreaded` member) - with an unresolvable external reference.
  Enabling RTTI for Jolt's own build matches it to this project's ambient default rather than punching a
  one-off `-fno-rtti` hole in a single atlas-physics translation unit, and costs only a documented, small
  simulation-speed difference (Jolt's own release notes: ~5% for MSVC, ~0% measured for clang) - Jolt itself
  never uses C++ `typeid`/`dynamic_cast` internally; its own from-scratch reflection system
  (`JPH_DECLARE_RTTI_VIRTUAL`, `JPH::Factory`) is unrelated to this flag.
- **`INTERPROCEDURAL_OPTIMIZATION ON`**, matching Jolt's own default and the HelloWorld reference - verified
  (not assumed) that this project's own sanitized debug preset (`-fsanitize=address,undefined`) configures,
  builds, and links cleanly with Jolt's own LTO enabled; no friction between Jolt's own LTO and this
  project's sanitizers was observed, so this did not need to be turned `OFF` for the sanitized preset
  specifically.
- **Threading** - `JPH::JobSystemSingleThreaded`, covered under "Scoping decisions (Jolt)" above.

`OBJECT_LAYER_BITS 16`, `DOUBLE_PRECISION OFF`, `GENERATE_DEBUG_SYMBOLS ON`, `FLOATING_POINT_EXCEPTIONS_ENABLED
OFF` are all taken unchanged from the HelloWorld reference - none of them raised a
project-specific concern the way the six above did. `TARGET_UNIT_TESTS`/`TARGET_HELLO_WORLD`/
`TARGET_PERFORMANCE_TEST`/`TARGET_SAMPLES`/`TARGET_VIEWER` are all explicitly forced `OFF` - Jolt's own
`Build/CMakeLists.txt` already gates all of these behind "are we the top-level project" (false here, since
Jolt is `FetchContent`'d as a subdirectory), but forcing them off explicitly avoids relying on that guard
never changing upstream. Jolt's own `Jolt::Jolt` CMake target is exempted from this project's clang-tidy gate
(`set_target_properties(Jolt PROPERTIES CXX_CLANG_TIDY "")`), mirroring `atlas-render`'s own SDL3 precedent
exactly - this project's static-analysis gate never reaches into a fetched dependency's own source.

## Verification (issue #178)

Performed against a scratch build directory (`build/debug-jolt-verify`, removed afterwards - not checked
in), never against a build directory this repository tracks:

- **`cmake --preset debug -DATLAS_PHYSICS_BACKEND=JOLT` + full build + `ctest`**: 746 tests passed, 0 failed
  (737 pre-existing tests, unaffected, plus 9 new `JoltPhysicsBackend` tests) - the project's full sanitized
  (`-fsanitize=address,undefined`) debug preset, Jolt's own `CROSS_PLATFORM_DETERMINISTIC`/
  `INTERPROCEDURAL_OPTIMIZATION` both `ON`.
- **`JoltPhysicsBackend.DynamicBodyFallsUnderGravityWithinPlausibleRange`'s actual measured numbers**: a
  `Dynamic` body starting at `y = 10.0`, stepped 60 times at a real `1.0F / 60.0F` timestep (1 simulated
  second), fell to `y = 5.0981` - a measured fall of **4.9019m**, against this test's own computed continuous-
  kinematics estimate of 4.9050m (`0.5 * 9.81 * 1.0^2`) and its asserted plausible band of `[2.4525, 7.3575]`
  (`±50%` around that estimate - see the test file's own doc comment for why that band is intentionally wide).
  The measured value landed within 0.06% of the continuous-kinematics estimate - strong, genuine evidence
  Jolt's own gravity integration is really running, not a coincidental non-zero delta.
- **Plain default (`NULL` physics backend / `NULL` render backend) clean build + `ctest`**: a separate scratch
  build directory, no `ATLAS_PHYSICS_BACKEND` override - 737 tests passed, 0 failed, confirming this issue's
  changes don't affect a build that never opts into Jolt.
- **`-DATLAS_RENDER_BACKEND=SDL3 -DATLAS_PHYSICS_BACKEND=JOLT` together**: builds and links cleanly (both
  third-party `FetchContent`s coexist without conflict), 783 tests passed, 0 failed (746 from the `JOLT`-only
  run above, plus SDL3's own additional real-backend tests) - confirms no interaction/link conflict between
  the two fetched third-party dependencies.
- **`clang-format --dry-run --Werror`** on every file this issue touched or created - clean.
- **`clang-tidy --warnings-as-errors=*`** against a `cmake --preset clang-tidy -DATLAS_ENABLE_CLANG_TIDY=OFF
  -DATLAS_PHYSICS_BACKEND=JOLT` compile-commands build, scoped to this issue's own new `.cpp`/test files
  (`jolt_physics_backend.cpp`, `atlas_physics.cpp`, `jolt_physics_backend_test.cpp`) - clean; Jolt's own
  `Jolt::Jolt` target is exempted from this gate (see above), the same way SDL3's targets are in
  `atlas-render/CMakeLists.txt`.

## Verification (issue #179)

Performed against scratch build directories (`build/debug-jolt-verify`, `build/debug-null-verify`,
`build/clang-tidy-verify`, all removed afterwards - not checked in), never against a build directory this
repository tracks:

- **`cmake --preset debug -DATLAS_PHYSICS_BACKEND=JOLT` + full build + `ctest`**: **764 tests passed, 0
  failed** (748 pre-existing, unaffected, plus 16 `JoltPhysicsBackend` tests - 9 from #178 plus 7 new: shape
  conversion for each of the four primitive kinds, the degenerate-convex-hull error path, the default-shape
  regression check, the collision-resolution settling test, and the bit-exact determinism test) - the
  project's full sanitized (`-fsanitize=address,undefined`) debug preset, Jolt's own
  `CROSS_PLATFORM_DETERMINISTIC`/`INTERPROCEDURAL_OPTIMIZATION` both `ON`.
- **`JoltPhysicsBackend.DynamicBodySettlesOnStaticFloorAndDoesNotFallThrough`'s actual measured numbers**: a
  `Dynamic` 0.5m-radius sphere starting at `y = 3.0`, dropped onto a `Static` 10m x 0.5m x 10m `BoxShape` floor
  centered at `y = -0.5` (top surface at `y = 0.0`, so the geometrically exact resting height is
  `y = 0.5`), stepped 180 times (3s simulated) at the real `1.0F / 60.0F` timestep, settled at **`y =
  0.480000`** - 0.02m (2cm) below the exact geometric prediction, matching Jolt's own default contact-
  resolution slop (`JPH::PhysicsSettings::mPenetrationSlop`/`mSpeculativeContactDistance`, both 0.02m by
  default) almost exactly, and well inside this test's own asserted ±0.05m tolerance band. A further 60 steps
  (1 more simulated second) left it at the identical `y = 0.480000` - zero drift - confirming it genuinely
  came to rest rather than continuing to sink or merely not having fallen through yet at the first check.
- **`JoltPhysicsBackend.IdenticalSetupAndStepsProduceBitExactIdenticalState`: confirmed bit-exact, not merely
  "the test exists."** Run standalone against the built library (a separate scratch harness, to capture the
  raw numbers this README quotes, not just the test's own pass/fail): both runs of the three-body scene
  (`Static` floor, `Dynamic` sphere, `Dynamic` box), each stepped 120 times, produced identical values down to
  every printed digit for every position and rotation component checked, e.g. the sphere settled at
  `x=-5.75808627e-08 y=0.4799999 z=0.250001103` in **both** runs, and the box at
  `x=1.99997175 y=0.479999721 z=-0.999964774` with rotation `(-5.91981575e-09, -0.000269640615,
  -1.32154412e-08, 1)` in **both** runs. The test itself asserts this with exact `EXPECT_EQ` (never
  `EXPECT_NEAR`/`EXPECT_FLOAT_EQ`) on every raw float component of every body, per this issue's own explicit
  instruction, and it passed - genuine bit-exact determinism, not a coincidentally-close approximation.
- **Plain default (`NULL` physics backend) clean build + `ctest`**: a separate scratch build directory, no
  `ATLAS_PHYSICS_BACKEND` override - **748 tests passed, 0 failed**, confirming this issue's changes don't
  affect a build that never opts into Jolt (748 = 764 minus the 16 Jolt-only tests, exactly as expected).
- **`clang-format --dry-run --Werror`** on every file this issue touched - clean.
- **`clang-tidy --warnings-as-errors=*`** against a `cmake --preset clang-tidy -DATLAS_ENABLE_CLANG_TIDY=OFF
  -DATLAS_PHYSICS_BACKEND=JOLT` compile-commands build, scoped to this issue's own changed `.cpp` files
  (`jolt_physics_backend.cpp`, `jolt_physics_backend_test.cpp`, `null_physics_backend_test.cpp`) - clean.
  Two real findings along the way, both fixed rather than suppressed: a redundant `.c_str()` call converting
  `JPH::String` to `std::string` (`readability-redundant-string-cstr` - fixed by constructing from
  `.data()`/`.size()` instead, since `JPH::String` and `std::string` are different `std::basic_string`
  instantiations with no cross-allocator converting constructor); and two test functions whose sheer count of
  sequential `EXPECT_EQ`/`EXPECT_NEAR` assertions tripped `readability-function-cognitive-complexity`
  (gtest's own macro expansion, not real branching logic) - fixed by factoring the repeated stepping loop and
  the bit-exact comparison into small, focused helper functions rather than reaching for a blanket `NOLINT`.

## Determinism

Unlike `atlas-render`/`atlas-audio` (presentation-only, explicitly excluded from the determinism boundary -
§4), `atlas-physics`'s output feeds back into simulation state: a rigid body's resolved position/rotation is
exactly the kind of state spec §4's bit-exact determinism guarantee covers. `NullPhysicsBackend` trivially
satisfies that guarantee (it does no computation at all - `step()` is a genuine no-op). `JoltPhysicsBackend`
is this library's first backend where determinism is a real, load-bearing build concern rather than a
trivial one: it is built with Jolt's own `CROSS_PLATFORM_DETERMINISTIC` option `ON` (see "Jolt CMake
integration" above) and never reads wall-clock time or unseeded entropy anywhere in its own code - `step()`'s
`delta_seconds` is exclusively the caller-supplied fixed timestep, exactly like every other `PhysicsBackend`.

**Issue #179 makes this a proven, not merely asserted, property**: `JoltPhysicsBackend.
IdenticalSetupAndStepsProduceBitExactIdenticalState` (see "Verification (issue #179)" above) constructs two
entirely separate `JoltPhysicsBackend` instances, creates an identical set of bodies in both (identical
`BodyCreateInfo`s, identical creation order - a `Static` floor plus two `Dynamic` bodies of different shapes),
steps both an identical number of times at an identical fixed timestep, and asserts the resulting `BodyState`
for every body is bit-for-bit identical between the two instances via exact `==`/`EXPECT_EQ` - deliberately
never a tolerance-based comparison, which would mask a real determinism violation. It passed: no
nondeterminism was found in this single-process, single-build, same-hardware scenario. Real cross-platform
bit-exactness of the full Jolt-driven simulation (verified identically across every one of this project's own
deployment targets, not just "configures with the flag on, and matches itself within one process") remains
future work - issue #176's own umbrella lists CI wiring/determinism-replay tests (#183) as a later, separate
step, not this issue's own scope.

## Dependency position

`atlas-physics` depends publicly on `atlas::core` (for `core::Vec3`/`core::Quaternion`) plus
`atlas_project_options`, following the `atlas-core` CMake pattern - and, once `ATLAS_PHYSICS_BACKEND=JOLT`,
publicly on Jolt's own `Jolt::Jolt` CMake target (since `jolt_physics_backend.hpp` includes real Jolt headers
directly, the same way `atlas-render`'s `Sdl3FrameBackend` publicly depends on SDL3). As of issue #178,
`atlas-physics` is a regular (non-`INTERFACE`) CMake library target - `src/atlas_physics.cpp` is now its one
always-present source file (see that file's own doc comment for exactly why a single target needed to stop
being `INTERFACE`), with `src/jolt_physics_backend.cpp` conditionally appended only under the `JOLT` backend,
mirroring `atlas-render`'s own CMakeLists.txt shape (always a regular library; SDL3-only sources appended
conditionally) rather than a target that changes kind per backend. Per §13, this library is optional in the
ordinary sense any capability library is (a game that doesn't need physics doesn't compose it), but it is
**not** client-only the way `atlas-input`/`atlas-ui`/`atlas-render` are: a server host authoritatively
simulating physics-affected state composes `atlas-physics` exactly as a client does. `atlas-physics`
deliberately does **not** depend on `atlas-render` - even though both libraries need the exact same
`Vec3`/`Quaternion` math types, that shared need is exactly why those types were relocated to `atlas-core`
(issue #176's own Part A) rather than `atlas-physics` picking up a dependency on `atlas-render` to reuse them,
which would have wrongly made a headless server's physics simulation depend on a presentation-only library.

## Open questions (flagging for human review, not silently resolved)

- **Real, configurable collision shapes - resolved by this issue (#179).** `BodyCreateInfo::shape` now
  carries a real box/sphere/capsule/convex-hull vocabulary, and `JoltPhysicsBackend` converts it into the
  matching real Jolt shape (see "Shape vocabulary (issue #179)" above). Any shape kind beyond these four
  (trimesh, heightfield, ...) remains a real, undesigned follow-up - not added speculatively here, per this
  issue's own explicit scope boundary.
- **Raycast/sweep query API** - #180's job, now that #179 gives the contract real, varied geometry (not just
  one hardcoded sphere) to query against.
- **Collision events via `JPH::ContactListener`** - #187's job (added to the umbrella after PR #186's review;
  complements #180's pull-based queries with a push-based "these bodies just touched" notification). Out of
  scope for this issue.
- **DAG-integration demo capability** - #188's job (also added after PR #186's review): `atlas-physics` stays
  a runtime library outside the capability DAG per §5, correctly, but nothing yet shows how a capability
  actually observes physics-simulated state via properly `depends_on`/`consumes`-ordered composition. This
  issue's own new tests exercise `JoltPhysicsBackend` directly, not through any capability - proving the
  mechanism, not the DAG-integration story #188 owns.
- **`Camera`/collision wiring** - #181/#182, downstream of both this contract and `atlas-render`'s own
  eventual `Camera` type; independent of this round entirely.
- **CI wiring** - #183, mirroring #161's rigor bar for `atlas-render`'s own CI integration; deliberately not
  touched by this issue per its own explicit scope boundary. Flagging one concrete thing #183 will likely need
  beyond ordinary CI wiring: a Linux CI runner fetching/building Jolt from source needs nothing beyond a C++
  compiler and CMake (no system package dependency was needed in this sandbox), but #183 should still verify
  this holds on every one of this project's own CI matrix legs (macOS/Windows included), not assume it from
  this one Linux sandbox's own experience.
- **Body-handle reuse policy for `JoltPhysicsBackend`** - deliberately kept at "monotonic, never reused" (see
  "Scoping decisions (Jolt)" above), matching `NullPhysicsBackend`; a slot-recycling policy remains a possible
  future refinement, not required by this issue's scope.
- **Cross-platform (cross-machine/cross-compiler) bit-exact determinism** - issue #179's own determinism test
  proves same-process, same-build, same-hardware bit-exactness (see "Determinism" above); genuinely verifying
  this holds *across* this project's own deployment targets (Linux/macOS/Windows, gcc/clang/MSVC) remains
  #183's job, not this issue's.

## References

- #176 (parent/umbrella: rigid-body physics + collision extension point)
- #177 (`PhysicsBackend` concept contract + `NullPhysicsBackend`)
- #178 (`JoltPhysicsBackend` bring-up - FetchContent, world init, real gravity-driven step loop, one
  hardcoded placeholder shape)
- #179 (this issue: real shape vocabulary, genuine collision resolution, bit-exact determinism test)
- [JoltPhysics](https://github.com/jrouwe/JoltPhysics) (MIT license, v5.6.0) /
  [JoltPhysicsHelloWorld](https://github.com/jrouwe/JoltPhysicsHelloWorld) (the reference FetchContent
  integration and world/body-init boilerplate this issue's `JoltPhysicsBackend` is modeled on)
