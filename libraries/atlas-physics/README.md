# atlas-physics

**Status:** Raycast/sweep query API added to the contract (issue #180, the fifth sub-issue of the umbrella
#176: rigid-body physics + collision extension point). Issue #177 implemented the compile-time
`PhysicsBackend` concept contract and the always-available `NullPhysicsBackend`; #178 added
`JoltPhysicsBackend`, the first backend that genuinely simulates, behind one hardcoded placeholder
collision shape; #179 replaced that placeholder with a real, backend-agnostic shape vocabulary
(box/sphere/capsule/convex hull), proved genuine collision resolution (a `Dynamic` body settling on a
`Static` floor rather than falling through it), and added this library's first bit-exact determinism test;
#180 adds `raycast()`/`sweep()` - the query-side counterpart to #179's simulation-side work, and the last
piece the contract needs before camera collision (#181/#182) can be built against it.

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
  `step(float) -> void`, `body_state(BodyId) -> std::optional<BodyState>`,
  `raycast(core::Vec3, core::Vec3, float) -> std::optional<HitResult>` (issue #180), and
  `sweep(const BodyShape&, core::Vec3, core::Quaternion, core::Vec3) -> std::optional<HitResult>` (issue
  #180). Mirrors `atlas::audio::AudioBackend` and `atlas::render::FrameBackend` exactly in shape and intent.
- **`atlas::physics::HitResult`** (`include/atlas/physics/body.hpp`) - the shared result type `raycast()`/
  `sweep()` both return (issue #180) - see "Raycast/sweep query API (issue #180)" below.
- **`atlas::physics::NullPhysicsBackend`** (`include/atlas/physics/null_physics_backend.hpp`) - the
  always-buildable `PhysicsBackend`: stores each created body's position/rotation in a plain vector and does
  nothing else (no forces, no collision, no shape simulation). Accepts `BodyCreateInfo::shape` (issue #179)
  exactly like every other field it doesn't act on. `raycast()`/`sweep()` (issue #180) always report
  `std::nullopt`, unconditionally - see its own doc comment for the exact index-allocation and
  generation-checking choices made (below, "Scoping decisions").
- **`atlas::physics::JoltPhysicsBackend`** (`include/atlas/physics/jolt_physics_backend.hpp`,
  `src/jolt_physics_backend.cpp`) - the first real `PhysicsBackend`: a genuine `JPH::PhysicsSystem`, stepped
  by a real fixed timestep, with every body backed by an actual Jolt `JPH::BodyID` rather than a
  locally-echoed pose. `create_body()` converts `BodyCreateInfo::shape` into a real `JPH::BoxShape`/
  `JPH::SphereShape`/`JPH::CapsuleShape`/`JPH::ConvexHullShape` (issue #179, replacing #178's own hardcoded
  placeholder sphere - see "Shape vocabulary (issue #179)" below). Fully real from `create_body()` through
  `step()` through `body_state()`, including genuine collision resolution between bodies (`tests/atlas-physics/
  jolt_physics_backend_test.cpp`'s `DynamicBodySettlesOnStaticFloorAndDoesNotFallThrough`), and now genuine
  `raycast()`/`sweep()` queries against that same real world (issue #180 - see "Raycast/sweep query API (issue
  #180)" below). Only compiled when `ATLAS_PHYSICS_BACKEND=JOLT`.
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

**Raycast/sweep query API - resolved by #180, now that this issue gave the contract real geometry to query
against.** See "Raycast/sweep query API (issue #180)" below for the full design.

## Raycast/sweep query API (issue #180)

The query-side counterpart to #179's simulation-side work: a point query along a ray (`raycast()`) and a
translation-only shape cast (`sweep()`), both added to the `PhysicsBackend` concept. This is the last piece
the contract needs before camera collision (#181/#182) can be built against it - #180's own text: "the
query-side counterpart to #179's simulation-side work, and the last piece the `PhysicsBackend` contract
(#177) needs before camera collision can be built against it."

**Shared result type: `HitResult`** (`include/atlas/physics/body.hpp`, placed there rather than a new header
since it is a small, plain aggregate reused by both queries, exactly the same reasoning `BodyState` already
follows):

```cpp
struct HitResult {
    BodyId body;
    core::Vec3 point;
    core::Vec3 normal;
};
```

`point` and `normal` are both world-space. `normal` points *away* from the hit surface (outward, back toward
whichever side the ray/sweep approached from) - this is Jolt's own documented convention for both query
kinds (see "Real Jolt API findings" below), so `HitResult` simply inherits it rather than picking an
arbitrary alternative convention and having to flip a sign at this contract's own boundary.

**Contract additions** (`include/atlas/physics/physics_backend.hpp`):

```cpp
{ backend.raycast(origin, direction, max_distance) } -> std::same_as<std::optional<HitResult>>;
{ backend.sweep(shape, from_position, from_rotation, to_position) } -> std::same_as<std::optional<HitResult>>;
```

- **`raycast(core::Vec3 origin, core::Vec3 direction, float max_distance)`** - `direction` is a caller-supplied
  direction, **not required to be pre-normalized**: this contract's own choice (the issue's own text left it
  open, "your call whether you require it pre-normalized... or normalize it yourself defensively") is the
  latter - `JoltPhysicsBackend::raycast()` defensively normalizes `direction` internally so the ray's actual
  reach is always exactly `max_distance` regardless of the magnitude the caller happened to pass. A
  zero-length (or otherwise degenerate, `<= 0`-length) `direction`, or a non-positive `max_distance`, reports
  `std::nullopt` rather than dividing by zero or forwarding a nonsensical ray to Jolt.
- **`sweep(const BodyShape& shape, core::Vec3 from_position, core::Quaternion from_rotation, core::Vec3
  to_position)`** - sweeps `shape` (the same backend-agnostic variant `BodyCreateInfo::shape` already uses,
  issue #179) from `from_position`/`from_rotation` to `to_position`, keeping rotation fixed throughout (a
  translation-only cast) - exactly what camera collision (#182) needs: moving a collision volume from a
  pivot point to a desired position without also rotating it in flight.
- Neither query sources anything internally (no clock, no hidden state) - purely a function of the world's
  current body state and the caller-supplied parameters, matching this contract's own `step()` discipline.

**`NullPhysicsBackend`'s `raycast()`/`sweep()` always report `std::nullopt`, unconditionally** - regardless of
what bodies exist or where they are positioned, mirroring its own "does nothing, echoes nothing back beyond
position/rotation" precedent applied to queries: this backend has no real shapes/geometry to test a query
against (bodies here are just an echoed-back position/rotation pair), so "nothing was hit" is the only honest
answer. Proven by `NullPhysicsBackend.RaycastAlwaysReturnsNulloptEvenAimedDirectlyAtABody` and
`NullPhysicsBackend.SweepAlwaysReturnsNulloptEvenAimedDirectlyAtABody` (`tests/atlas-physics/
null_physics_backend_test.cpp`) - a body positioned exactly where a real backend would obviously report a
hit, and this backend still reports `std::nullopt`.

### Real Jolt API findings (issue #180)

Investigated directly against the real fetched Jolt source (v5.6.0) rather than assumed - the pre-issue
verified-facts brief turned out accurate on every point; nothing needed correcting, only confirming:

- **Raycasts use `JPH::NarrowPhaseQuery::CastRay`'s simple closest-hit overload** (`bool CastRay(const
  RRayCast&, RayCastResult&, ...) const`, `Jolt/Physics/Collision/NarrowPhaseQuery.h`) - no collector needed,
  confirmed by Jolt's own `Samples/SamplesApp.cpp` `EProbeMode::Ray` case, which uses exactly this overload.
- **`RRayCast`'s `mDirection` length *is* the ray's max distance** (`Jolt/Physics/Collision/RayCast.h`:
  "Direction and length of the ray (anything beyond this length will not be reported as a hit)") - confirmed
  directly in the header, not assumed. `JoltPhysicsBackend::raycast()` builds this by normalizing the
  caller-supplied `direction` and scaling by `max_distance`.
- **`RayCastResult`** (`Jolt/Physics/Collision/CastResult.h`) carries `mBodyID` and `mFraction` (inherited
  from `BroadPhaseCastResult`) plus its own `mSubShapeID2` - exactly the three fields expected. The hit point
  is `ray.GetPointOnRay(hit.mFraction)` (`RayCastT::GetPointOnRay`, "Get point with fraction inFraction on
  ray") - Jolt's own documented convention, used as-is.
- **The hit normal comes from `Body::GetWorldSpaceSurfaceNormal(subShapeID, position)` on the locked hit
  body** - confirmed as Jolt's own idiomatic pattern by `Samples/SamplesApp.cpp`'s `EProbeMode::Ray` case
  (`Vec3 normal = hit_body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, outPosition);`) and by
  `NarrowPhaseQuery::CastRay`'s own doc comment ("If you want the surface normal of the hit use
  `Body::GetWorldSpaceSurfaceNormal(...)`"). `JoltPhysicsBackend::raycast()` locks the hit body via
  `JPH::BodyLockRead` (falling back to a zero normal if the lock somehow fails, which cannot happen in this
  single-threaded backend but `BodyLockRead`'s own contract requires checking `Succeeded()` regardless)
  rather than inventing a different mechanism.
- **Sweeps have no simple closest-hit overload** - `NarrowPhaseQuery::CastShape` is collector-based only
  (`void CastShape(const RShapeCast&, const ShapeCastSettings&, RVec3Arg, CastShapeCollector&, ...) const`),
  confirmed directly against the header (no overload returning a single result exists, unlike `CastRay`).
  `JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>` is Jolt's own idiomatic way to get "just the
  closest hit" - confirmed by several of Jolt's own `Samples/Tests` (`MotorcycleTest.cpp`'s gravity-override
  probe, `ShapeFilterTest.cpp`, `VehicleConstraintTest.cpp`), all of which construct exactly this collector
  type for exactly this purpose.
- **`RShapeCast::sFromWorldTransform(shape, scale, worldTransform, direction)`** is Jolt's own documented,
  recommended constructor ("Construct a shape cast using a world transform for a shape instead of a center of
  mass transform") - used as-is rather than hand-assembling `mCenterOfMassStart` from the shape's own center
  of mass. `mDirection`'s length is the cast's max distance, the same convention as `RRayCast` (confirmed in
  the same header, `Jolt/Physics/Collision/ShapeCast.h`) - `JoltPhysicsBackend::sweep()` computes this
  directly as `to_position - from_position` (a translation-only cast has no separate "max distance" the way
  `raycast()` does - the sweep's own end position fixes it).
  `NarrowPhaseQuery::CastShape`'s own doc comment on `inBaseOffset` ("can be zero to get results in world
  position, but when you're testing far from the origin you get better precision by picking a position
  that's closer e.g. `inShapeCast.mCenterOfMassStart.GetTranslation()`") is followed as-is: the sweep's own
  start translation is passed as `inBaseOffset`, and the world-space hit point is recovered by adding it back
  to the collector's own (offset-relative) result.
- **`ShapeCastResult`/`CollideShapeResult`** (`Jolt/Physics/Collision/ShapeCast.h` /
  `Jolt/Physics/Collision/CollideShape.h`) carry `mFraction`, `mContactPointOn1`/`mContactPointOn2`,
  `mPenetrationAxis`, and `mBodyID2` - exactly the fields expected. `JoltPhysicsBackend::sweep()` uses
  `mContactPointOn2` (the contact point on the *hit body's* surface - `HitResult::point`'s own documented
  convention) and derives the normal as `(-mPenetrationAxis).Normalized()`, exactly matching
  `CollideShapeResult`'s own doc comment ("Direction to move shape 2 out of collision along the shortest
  path... You can use `-mPenetrationAxis.Normalized()` as contact normal") - confirmed correct empirically too
  (see "Verification" below): the reported normal points away from the hit surface, matching `HitResult`'s
  own documented convention exactly.
- **`JoltPhysicsBackend::body_id_from_jolt()`** - the first reverse lookup this backend has ever needed
  (`bodies_` was, until now, only ever consulted forward, `BodyId::index -> JPH::BodyID`). A linear scan over
  `bodies_`, mirroring its own existing precedent (a plain, monotonically-growing vector, never reusing an
  index) - a hash map was considered and deliberately not reached for, given this round's scope never
  exercises more than a handful of bodies in any of its own tests (see "Open questions" below for what would
  justify one).

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

## Verification (issue #180)

Performed against scratch build directories (`build/debug-jolt-verify`, `build/debug-null-verify`,
`build/clang-tidy-verify`, all removed afterwards - not checked in), never against a build directory this
repository tracks:

- **`cmake --preset debug -DATLAS_PHYSICS_BACKEND=JOLT` + full build + `ctest`**: **774 tests passed, 0
  failed** (764 pre-existing, unaffected, plus 10 new tests: `NullPhysicsBackend`'s two always-`std::nullopt`
  query tests, and `JoltPhysicsBackend`'s raycast-hits/raycast-non-unit-direction/raycast-aimed-away/
  raycast-too-short/raycast-zero-direction/sweep-hits/sweep-nothing-in-path/raycast-determinism tests) - the
  project's full sanitized (`-fsanitize=address,undefined`) debug preset, Jolt's own
  `CROSS_PLATFORM_DETERMINISTIC`/`INTERPROCEDURAL_OPTIMIZATION` both `ON`.
- **`JoltPhysicsBackend.RaycastHitsRealStaticBoxWithCorrectBodyPointAndNormal`'s actual measured numbers**: a
  1m half-extent `Static` `BoxShape` centered at `(0, 0, 5)` (near face at exactly `z = 4.0`), a ray fired from
  the origin straight down `+Z` for up to 10m. Measured, via a standalone scratch harness built against the
  actual compiled library (mirroring #179's own "capture the raw numbers, not just pass/fail" practice):
  **hit point `(0.00000000, 0.00000000, 4.00000000)`, hit normal `(0.00000000, 0.00000000, -1.00000000)`** -
  bit-for-bit identical to the geometrically exact prediction (the ray meets the box's near face dead center;
  the outward normal points back down `-Z`, toward the ray's own origin), not merely "within tolerance." The
  gtest assertion itself uses a `1.0e-4` tolerance (a raycast against a single convex shape is exact GJK
  ray-vs-convex intersection, with none of the contact-resolution slop a real settling-collision test needs to
  tolerate) - the measured values happened to match the prediction exactly.
- **`JoltPhysicsBackend.SweepHitsRealStaticBoxBeforeReachingItsFarSide`'s actual measured numbers**: the same
  box, a 0.5m-radius sphere swept from the origin to `(0, 0, 10)` (2m past the box's own far face at
  `z = 6.0`). Measured via the same standalone harness: **hit point `(0.00000000, 0.00000000, 4.00000000)`,
  hit normal `(0.00000000, 0.00000000, -1.00000000)`** - again bit-for-bit identical to the geometrically
  exact prediction (the swept sphere's surface first touches the box's near face dead center at `z = 4.0`,
  not the box's own center at `z = 5.0` nor the sweep's end position at `z = 10.0`). The gtest assertion uses
  a `0.05` tolerance (Jolt's own default contact/speculative-contact margin, ~0.02m, doubled - the same
  margin #179's own settling-collision test tolerates) - the measured values again matched exactly.
- **`JoltPhysicsBackend.IdenticalRaycastQueryProducesBitExactIdenticalHitResult`: confirmed bit-exact.** An
  identical scene (one `Static` `BoxShape`) queried with identical raycast parameters on two entirely separate
  `JoltPhysicsBackend` instances produced an identical `HitResult` - same `BodyId`, and every raw float
  component of `point`/`normal` bit-for-bit equal via exact `EXPECT_EQ` (never `EXPECT_NEAR`/`EXPECT_FLOAT_EQ`,
  per §4's own bit-exact guarantee applied to queries exactly as much as to `step()`) - and it passed.
- **`NullPhysicsBackend.RaycastAlwaysReturnsNulloptEvenAimedDirectlyAtABody`/
  `SweepAlwaysReturnsNulloptEvenAimedDirectlyAtABody`**: a `Static` `BoxShape` body positioned exactly where a
  real backend (see the two tests immediately above) genuinely reports a hit - `NullPhysicsBackend` still
  reports `std::nullopt` for both queries, confirming the documented "no real geometry to test against, so
  nothing is ever hit" behavior with a real, adversarial-looking body present, not merely an empty world.
- **Plain default (`NULL` physics backend) clean build + `ctest`**: a separate scratch build directory, no
  `ATLAS_PHYSICS_BACKEND` override - **750 tests passed, 0 failed** (774 minus the 24 Jolt-only tests, exactly
  as expected), confirming this issue's changes don't affect a build that never opts into Jolt.
- **`clang-format --dry-run --Werror`** on every file this issue touched - clean.
- **`clang-tidy --warnings-as-errors=*`** against a `cmake --preset clang-tidy -DATLAS_ENABLE_CLANG_TIDY=OFF
  -DATLAS_PHYSICS_BACKEND=JOLT` compile-commands build, scoped to this issue's own changed `.cpp` files
  (`jolt_physics_backend.cpp`, `jolt_physics_backend_test.cpp`, `null_physics_backend_test.cpp`) - clean.
  Findings along the way, all fixed rather than blanket-suppressed:
  - `bugprone-unchecked-optional-access` on `body_id_from_jolt()`'s `bodies_[index].has_value() &&
    *bodies_[index] == ...` - the checker couldn't tell the two `operator[]` calls referred to the same
    element; fixed by binding a local `const std::optional<JPH::BodyID>&` once and checking/dereferencing
    that, mirroring `destroy_body()`'s own existing `stored` local exactly.
  - `readability-convert-member-functions-to-static`/`bugprone-easily-swappable-parameters` on
    `NullPhysicsBackend::raycast()`/`sweep()` - both are real, considered findings rather than noise (the
    methods genuinely touch no instance state, and `origin`/`direction` genuinely are two same-typed,
    adjacent parameters), but neither is a fix that improves this code: `readability-static-accessed-through-
    instance` would then fire at every call site if made `static` (and worse, would make this backend's own
    query call *syntax* diverge from `JoltPhysicsBackend`'s - the whole point of the shared `PhysicsBackend`
    concept is that `backend.raycast(...)` reads identically regardless of concrete backend), and the
    parameter order is this contract's own fixed signature (physics_backend.hpp), not a choice
    `NullPhysicsBackend` can unilaterally restructure. Both are `NOLINT`ed with a one-line reason each,
    per CLAUDE.md's own "fix the issue or add a `NOLINT(check-name)` with a one-line reason" - a considered
    exception, not a suppressed one.

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

**Issue #180 extends the same proof to queries**: §4's bit-exact determinism guarantee applies to a query
exactly as much as to `step()` - `JoltPhysicsBackend.IdenticalRaycastQueryProducesBitExactIdenticalHitResult`
(see "Verification (issue #180)" above) applies the identical two-instances/identical-parameters/exact-`==`
methodology to `raycast()`, reusing the same `expect_position_bit_exact()` helper #179's own determinism test
already introduced (rather than duplicating it) since both check the identical thing - "are these three raw
float components exactly equal" - regardless of whether they came from a `BodyState` or a `HitResult`. It
passed: no nondeterminism was found. The same cross-platform caveat as #179's own determinism test applies
unchanged here (same-process/same-build/same-hardware only, cross-platform bit-exactness remains #183's job).

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
- **Raycast/sweep query API - resolved by this issue (#180).** `raycast()`/`sweep()` are now part of the
  `PhysicsBackend` concept, implemented by both `NullPhysicsBackend` (always `std::nullopt`) and
  `JoltPhysicsBackend` (genuine `JPH::NarrowPhaseQuery::CastRay`/`CastShape` queries against the real world) -
  see "Raycast/sweep query API (issue #180)" above for the full design and "Real Jolt API findings (issue
  #180)" for the investigation. Overlap/volume queries, or any query shape beyond raycast + sweep, remain a
  real, undesigned follow-up per this issue's own explicit scope boundary - not added speculatively here.
- **Collision events via `JPH::ContactListener`** - #187's job (added to the umbrella after PR #186's review;
  complements #180's own pull-based queries, now implemented, with a push-based "these bodies just touched"
  notification). Out of scope for this issue.
- **DAG-integration demo capability** - #188's job (also added after PR #186's review): `atlas-physics` stays
  a runtime library outside the capability DAG per §5, correctly, but nothing yet shows how a capability
  actually observes physics-simulated state (or issues a raycast/sweep query) via properly
  `depends_on`/`consumes`-ordered composition. This issue's own new tests exercise `JoltPhysicsBackend`/
  `NullPhysicsBackend` directly, not through any capability - proving the mechanism, not the DAG-integration
  story #188 owns.
- **`Camera`/collision wiring** - #181/#182, now unblocked by this issue: #182 (camera collision) can build
  directly against `sweep()` (sweeping the camera's own collision volume from a pivot point to a desired
  position, exactly `sweep()`'s own documented use case) once #181 (the `Camera` type itself, independent of
  physics) exists. Neither sub-issue's own implementation is this issue's job.
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
- **`JoltPhysicsBackend`'s reverse `JPH::BodyID -> BodyId` lookup (`body_id_from_jolt()`, issue #180) is a
  linear scan over `bodies_`, deliberately not a hash map.** This round's scope never exercises more than a
  handful of bodies in any test, so the simplest correct approach was chosen over one with more moving parts
  to get right (e.g. keeping a `JPH::BodyID`-keyed map in sync with every `create_body()`/`destroy_body()`
  call). A capability that creates and queries against a genuinely large number of bodies per tick (#188's own
  eventual DAG-integration demo, or a real game beyond this project's own scope) is the concrete trigger that
  would justify revisiting this - not something to speculatively optimize for here.

## References

- #176 (parent/umbrella: rigid-body physics + collision extension point)
- #177 (`PhysicsBackend` concept contract + `NullPhysicsBackend`)
- #178 (`JoltPhysicsBackend` bring-up - FetchContent, world init, real gravity-driven step loop, one
  hardcoded placeholder shape)
- #179 (real shape vocabulary, genuine collision resolution, bit-exact determinism test)
- #180 (this issue: raycast/sweep query API on the `PhysicsBackend` contract)
- #181 / #182 (the camera-collision sub-issues this query API exists to serve, downstream of this issue)
- [JoltPhysics](https://github.com/jrouwe/JoltPhysics) (MIT license, v5.6.0) /
  [JoltPhysicsHelloWorld](https://github.com/jrouwe/JoltPhysicsHelloWorld) (the reference FetchContent
  integration and world/body-init boilerplate this issue's `JoltPhysicsBackend` is modeled on) -
  `Jolt/Physics/Collision/NarrowPhaseQuery.h`, `Jolt/Physics/Collision/RayCast.h`,
  `Jolt/Physics/Collision/ShapeCast.h`, `Jolt/Physics/Collision/CastResult.h`,
  `Jolt/Physics/Collision/CollideShape.h` (the real Jolt query API headers investigated for issue #180) and
  `Samples/SamplesApp.cpp`/`Samples/Tests/Vehicle/MotorcycleTest.cpp` (Jolt's own idiomatic raycast/sweep
  usage patterns this issue's `JoltPhysicsBackend::raycast()`/`sweep()` are modeled on)
