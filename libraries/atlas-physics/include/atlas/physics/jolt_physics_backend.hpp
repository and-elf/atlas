#pragma once

#include "atlas/physics/body.hpp"
#include "atlas/physics/body_id.hpp"
#include "atlas/physics/physics_backend.hpp"

// Real, third-party Jolt Physics headers (issue #178) - only compiled/visible
// when ATLAS_PHYSICS_BACKEND=JOLT (libraries/atlas-physics/CMakeLists.txt),
// mirroring atlas::render::Sdl3FrameBackend's own precedent of including its
// third-party backend's real headers directly in its own public header
// rather than hiding them behind a pimpl. Jolt.h must come first, before any
// other Jolt header (Jolt's own documented requirement - it defines macros,
// e.g. JPH_ASSERT, the other headers use unconditionally). This project's
// own .clang-format resolves to IncludeBlocks: Regroup (confirmed via
// `clang-format -dump-config`, not the LLVM-style-default Preserve a blank
// line alone would rely on), which reorders every #include alphabetically
// regardless of blank-line grouping - so a plain blank line between Jolt.h
// and the rest is not sufficient on its own and was a real, reproduced build
// failure (JPH_ASSERT undeclared) when a prior version of this file let
// clang-format re-sort Jolt.h after Jolt/Core/...  The clang-format off/on
// guard below is the standard, documented way to keep an include order
// clang-format would otherwise disturb.
// clang-format off
#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

// job_system_ (below) is a std::unique_ptr<JPH::JobSystem> (issue #193) -
// this header never needs to name either concrete job-system type
// (JPH::JobSystemSingleThreaded / JPH::JobSystemThreadPool): Jolt/Physics/
// PhysicsSystem.h above already pulls in a complete Jolt/Core/JobSystem.h
// transitively (via Jolt/Physics/PhysicsUpdateContext.h - verified against
// the real fetched Jolt source rather than assumed), and JPH::JobSystem has
// a `virtual ~JobSystem() = default` (Jolt/Core/JobSystem.h) - a genuinely
// safe, polymorphic base to own and destroy through a base-class pointer.
// Which concrete job system this instance actually constructed is this
// class's own .cpp-file implementation detail (make_job_system(), issue
// #193's own ThreadingMode), exactly mirroring how make_jolt_shape() already
// keeps every concrete JPH::Shape subtype an implementation detail this
// header never needs to name either.
namespace atlas::physics {

// A caller's choice of Jolt job-system implementation (issue #193) - a
// small, backend-agnostic config value (no JPH:: type appears in this
// enum's or JoltPhysicsBackendConfig's own signature, even though
// JoltPhysicsBackend's constructor obviously converts it into a genuine
// JPH::JobSystemSingleThreaded or JPH::JobSystemThreadPool internally - see
// the .cpp file's own make_job_system()), mirroring atlas::render::
// Sdl3FrameBackend's own DistanceCullConfig constructor-parameter precedent
// (issue #156): a caller-configurable-at-construction value with a default
// that exactly reproduces this backend's own pre-#193 hardcoded behavior, so
// no existing call site needs to change unless it opts in.
//
// SingleThreaded (default) - JPH::JobSystemSingleThreaded, #178's own
// original bring-up choice: every job runs synchronously on the calling
// thread, nothing to reason about beyond ordinary single-threaded control
// flow.
//
// ThreadPool - JPH::JobSystemThreadPool, genuinely parallelizing Jolt's own
// job graph across worker threads. See this file's own .cpp doc comment
// ("Threading mode (issue #193)") and this library's README ("Determinism
// investigation (issue #193)") for the real, investigated finding this
// choice rests on: Jolt's own documentation (Docs/Architecture.md,
// "Deterministic Simulation") states the simulation itself remains
// deterministic under CROSS_PLATFORM_DETERMINISTIC regardless of thread
// count, as long as the API calls that modify it happen in the same order -
// multi-threading only affects the *ordering* of broadphase queries,
// listener callbacks, and PhysicsSystem::GetActiveBodies, none of which this
// backend uses.
enum class ThreadingMode : std::uint8_t { SingleThreaded, ThreadPool };

// The full startup-configurable surface JoltPhysicsBackend's constructor
// accepts (issue #193). Every field defaults to this backend's own pre-#193
// hardcoded behavior exactly, so `JoltPhysicsBackend backend;` (this
// library's own existing tests, none of which needed to change for this
// issue) keeps compiling and behaving identically.
struct JoltPhysicsBackendConfig {
    // See ThreadingMode's own doc comment above.
    ThreadingMode threading_mode = ThreadingMode::SingleThreaded;

    // Only consulted when threading_mode == ThreadPool. -1 (default) is
    // JPH::JobSystemThreadPool's own documented sentinel for "auto-detect
    // the number of CPUs" (Jolt/Core/JobSystemThreadPool.h's own doc comment
    // on its inNumThreads constructor parameter, verified against the real
    // fetched Jolt source) - passed straight through, never itself
    // interpreted by this backend.
    int thread_count = -1;

    // This instance's fixed body budget - JPH::PhysicsSystem::Init()'s own
    // inMaxBodies parameter. 1024 (default) is #178's own original hardcoded
    // value (this issue's own text: "confirmed a reasonable default, not
    // something this issue needs to re-justify"), kept unchanged as the
    // default so no existing call site's behavior changes unless it opts
    // into a different value.
    //
    // max_body_pairs/max_contact_constraints (this file's own .cpp)
    // deliberately do NOT move together with max_bodies - they stay fixed at
    // 1024 each. Those two budgets size Jolt's own *contact* bookkeeping
    // (how many simultaneously-colliding body pairs / contact constraints
    // this instance can track at once), which depends on scene contact
    // density, not on body count - a game with 10,000 bodies that never
    // touch each other needs no more contact-tracking budget than one with
    // 10; conversely a caller genuinely needing more contact budget than a
    // caller needing more bodies is a real, independent, undesigned
    // follow-up (this library's README, "Open questions"), not something to
    // speculatively wire up here just because max_bodies grew a constructor
    // parameter this round.
    std::uint32_t max_bodies = 1024;
};

// The first real (non-null) atlas::physics::PhysicsBackend (issue #178, the
// first slice of #176 to actually simulate anything): a genuine Jolt Physics
// JPH::PhysicsSystem, stepped by a real fixed timestep. Satisfies the same
// PhysicsBackend concept NullPhysicsBackend does (physics_backend.hpp) - a
// caller composing a simulation loop never branches on which concrete
// backend it was handed.
//
// Scope: create_body() converts BodyCreateInfo::shape (body.hpp's own
// backend-agnostic BodyShape variant, issue #179) into a real Jolt shape of
// the matching kind - JPH::BoxShape/JPH::SphereShape/JPH::CapsuleShape/
// JPH::ConvexHullShape - via std::visit (see this class's own .cpp file,
// make_jolt_shape()). Issue #178's own hardcoded placeholder (every body got
// the exact same fixed-radius sphere regardless of BodyCreateInfo) is gone;
// see this library's README "Scoping decisions (Jolt)" for the full history.
// This is fully real from create_body() through step() through body_state() -
// a Dynamic body genuinely falls under Jolt's own gravity integration and
// genuinely collides with and rests on a Static body's real shape, a Static
// body genuinely never moves - rather than a half-real backend that runs a
// real Jolt world but only echoes back whatever pose was passed to
// create_body().
//
// raycast()/sweep() (issue #180) query this same real world: raycast() via
// JPH::NarrowPhaseQuery::CastRay's simple closest-hit overload, sweep() via
// JPH::NarrowPhaseQuery::CastShape plus Jolt's own
// JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> (CastShape has
// no simple closest-hit overload of its own, unlike CastRay - verified
// against the real Jolt source, see this class's own .cpp file). Both are
// genuinely real: a raycast against a body this instance actually created
// reports that body's real BodyId, a real world-space hit point derived from
// Jolt's own hit fraction, and a real surface normal (Body::
// GetWorldSpaceSurfaceNormal() for raycasts, -CollideShapeResult::
// mPenetrationAxis.Normalized() for sweeps - both Jolt's own documented
// convention, not invented here).
//
// An encapsulated class, not a basic aggregate (unlike this library's other
// value types): it owns a real JPH::PhysicsSystem plus the supporting
// allocator/job-system/layer-filter objects Jolt requires, with a genuine
// invariant to protect (every body added to the world must be removed and
// destroyed exactly once, in the right order, before the world itself goes
// away) - exactly the kind of invariant CLAUDE.md's Rule of Zero note
// carves out an exception for (cf. atlas::entity::EntityRegistry,
// atlas::render::Sdl3FrameBackend).
//
// Construction failure: unlike Sdl3FrameBackend (which fails whenever this
// machine has no GPU/display - the common case on a headless CI runner),
// nothing in this constructor's sequence (RegisterDefaultAllocator(),
// JPH::Factory construction, RegisterTypes(), JPH::PhysicsSystem::Init())
// has an equivalent hardware dependency for CPU-side physics, and
// JPH::PhysicsSystem::Init() itself returns void with no documented failure
// path. This constructor is therefore documented as realistically
// non-failing in practice (see the .cpp file's own doc comment for the full
// investigation) and does not throw - there is deliberately no "construction
// failure" test the way Sdl3FrameBackend's own test suite has one.
//
// create_body() can still fail in principle: JPH::BodyInterface::
// CreateAndAddBody returns an invalid JPH::BodyID once this instance's fixed
// body budget, max_bodies, is exhausted, and (issue #179) a
// BodyCreateInfo::ConvexHullShape whose points can't form a valid hull (e.g.
// too few, or degenerate/collinear) makes JPH::ConvexHullShapeSettings::
// Create() report an error rather than returning a shape. Both are genuine
// failures reported by throwing std::runtime_error, per this project's
// established std::expected-incompatibility convention (CLAUDE.md).
//
// Process-wide global Jolt state (JPH::RegisterDefaultAllocator(),
// JPH::Factory::sInstance, JPH::RegisterTypes()) is initialized exactly once
// per process (a function-local static guard in the .cpp file, not a
// per-instance concern) and deliberately never torn down (no
// JPH::UnregisterTypes() / delete JPH::Factory::sInstance call anywhere in
// this class) - see the .cpp file's own doc comment for why: Jolt's own
// RegisterTypes() populates process-wide collision-dispatch tables with no
// reference count of its own, so unregistering it while any other
// JoltPhysicsBackend instance might still be alive (e.g. across this
// library's own test binary, which constructs a fresh instance per TEST())
// would be unsafe. The resulting one-time, bounded "leak" (one JPH::Factory,
// for the lifetime of the process) is a deliberate, documented trade-off,
// not an oversight.
//
// Non-copyable and non-movable: JPH::PhysicsSystem itself derives from
// JPH::NonCopyable and is not designed to be relocated once JPH::
// PhysicsSystem::Init() has bound it to this instance's own broad-phase/
// object-layer filter members (which Init() takes by reference and requires
// to outlive the JPH::PhysicsSystem, per Jolt's own documented contract) -
// unlike Sdl3FrameBackend, nothing in this round's scope needs this type
// returned by value or stored by value in a container, so this is the
// simplest correct choice rather than an oversight.
class JoltPhysicsBackend {
public:
    // config's every field defaults to this backend's own pre-#193 hardcoded
    // behavior exactly (JoltPhysicsBackendConfig's own doc comment above) -
    // `JoltPhysicsBackend backend;` remains valid and unchanged in behavior.
    explicit JoltPhysicsBackend(JoltPhysicsBackendConfig config = {});
    ~JoltPhysicsBackend();

    JoltPhysicsBackend(const JoltPhysicsBackend&) = delete;
    JoltPhysicsBackend& operator=(const JoltPhysicsBackend&) = delete;
    JoltPhysicsBackend(JoltPhysicsBackend&&) = delete;
    JoltPhysicsBackend& operator=(JoltPhysicsBackend&&) = delete;

    // Throws std::runtime_error if this instance's fixed body budget
    // (JoltPhysicsBackendConfig::max_bodies, above - caller-configurable
    // since issue #193) is exhausted - JPH::BodyInterface::
    // CreateAndAddBody reports that by returning an invalid JPH::BodyID
    // rather than throwing itself - or if create_info.shape is a
    // ConvexHullShape whose points JPH::ConvexHullShapeSettings::Create()
    // rejects as not forming a valid hull.
    [[nodiscard]] BodyId create_body(const BodyCreateInfo& create_info);

    // A no-op for an already-destroyed or never-created body id, mirroring
    // NullPhysicsBackend's own "destroy is idempotent, never throws" contract.
    void destroy_body(BodyId body) noexcept;

    // Never sources delta_seconds internally - see physics_backend.hpp's own
    // doc comment on why a PhysicsBackend must never read a clock.
    void step(float delta_seconds);

    [[nodiscard]] std::optional<BodyState> body_state(BodyId body) const noexcept;

    // Casts a ray from `origin` in `direction` (need not be pre-normalized -
    // see physics_backend.hpp's own doc comment) for up to `max_distance`,
    // reporting the closest hit, or std::nullopt if nothing was hit (or
    // `direction` is degenerate/zero-length, or max_distance <= 0). Uses
    // JPH::NarrowPhaseQuery::CastRay's simple closest-hit overload (issue
    // #180 - see this class's own .cpp file for the full investigation of
    // why this is the right Jolt entry point).
    [[nodiscard]] std::optional<HitResult>
    raycast(core::Vec3 origin, core::Vec3 direction, float max_distance) const;

    // Sweeps `shape` (this library's own backend-agnostic BodyShape variant,
    // issue #179) from `from_position` to `to_position`, keeping
    // `from_rotation` fixed throughout (a translation-only cast), reporting
    // the closest hit, or std::nullopt if nothing was hit. Uses
    // JPH::NarrowPhaseQuery::CastShape plus Jolt's own
    // JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> (issue #180
    // - see this class's own .cpp file for the full investigation).
    [[nodiscard]] std::optional<HitResult> sweep(const BodyShape& shape,
                                                 core::Vec3 from_position,
                                                 core::Quaternion from_rotation,
                                                 core::Vec3 to_position) const;

private:
    // Minimal two-layer (non-moving / moving) broadphase/object-layer setup
    // Jolt requires before any body can be created - mirrors the official
    // JoltPhysicsHelloWorld example's own BPLayerInterfaceImpl exactly (see
    // the .cpp file's doc comment for the source this was modeled on): one
    // broadphase layer per object layer, since this round's scope has
    // nothing finer-grained to justify more.
    class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BroadPhaseLayerInterfaceImpl();

        [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override;
        [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

        // Only pure virtual (and therefore only needs overriding) when Jolt
        // is built with profiling/external-profiler support compiled in
        // (JPH_EXTERNAL_PROFILE or JPH_PROFILE_ENABLED - the latter is on by
        // default in Jolt's own Debug/Release configurations, including this
        // project's own sanitized debug preset) - always overridden here
        // regardless, since providing it unconditionally is harmless and
        // avoids this type's own definition needing to track Jolt's build
        // configuration.
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

    private:
        // `{}` value-initializes every element (JPH::BroadPhaseLayer's own
        // default constructor is `= default`, so without this every element
        // starts genuinely uninitialized) - the constructor body below then
        // overwrites both entries; this default member initializer exists so
        // that "before the constructor body runs" is never a state with
        // unspecified content, satisfying cppcoreguidelines-pro-type-member-init.
        std::array<JPH::BroadPhaseLayer, 2> object_to_broad_phase_{};
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override;
    };

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
    public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override;
    };

    // Guarantees the process-wide global Jolt registration (RegisterDefaultAllocator()
    // in particular) has already run before any other member below is
    // constructed - must stay the *first* declared member. JPH::
    // TempAllocatorImpl's own constructor (temp_allocator_ below) calls
    // straight through Jolt's global JPH::Allocate/AlignedAllocate function
    // pointers, which are null until RegisterDefaultAllocator() sets them;
    // since member initialization runs in declaration order regardless of
    // this class's own constructor body or member-initializer-list order,
    // calling ensure_global_jolt_init() from inside the constructor body
    // (i.e. after every member below has already been constructed) is too
    // late - this was a real, reproduced null-function-pointer crash inside
    // JPH::TempAllocatorImpl::TempAllocatorImpl, not a hypothetical ordering
    // concern.
    struct GlobalJoltInit {
        GlobalJoltInit();
    };
    GlobalJoltInit global_jolt_init_;

    // Declared before physics_system_ so C++'s reverse-member-destruction-
    // order destroys them *after* physics_system_ - JPH::PhysicsSystem::Init()
    // takes these three by reference and Jolt's own documentation requires
    // them to outlive the JPH::PhysicsSystem they were registered with.
    BroadPhaseLayerInterfaceImpl broad_phase_layer_interface_;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_layer_filter_;
    ObjectLayerPairFilterImpl object_layer_pair_filter_;

    // Only used transiently, inside step()'s own JPH::PhysicsSystem::Update()
    // call - no ordering relationship with physics_system_ to protect.
    JPH::TempAllocatorImpl temp_allocator_;

    // A JPH::JobSystemSingleThreaded or JPH::JobSystemThreadPool (issue
    // #193, this instance's own JoltPhysicsBackendConfig::threading_mode),
    // owned polymorphically through JPH::JobSystem's own base pointer - see
    // this header's own top-of-file doc comment for why a std::unique_ptr to
    // an otherwise-incomplete-here base type is safe given this class's
    // destructor is defined out-of-line in the .cpp file, and make_job_system()
    // there for which concrete type gets constructed. Both concrete types
    // must coexist as this same member type at runtime - a plain concrete
    // JPH::JobSystemSingleThreaded member (this backend's own pre-#193
    // shape) can no longer hold either.
    std::unique_ptr<JPH::JobSystem> job_system_;

    JPH::PhysicsSystem physics_system_;

    // This backend's own BodyId::index -> JPH::BodyID map. Mirrors
    // NullPhysicsBackend's own `bodies` vector precedent exactly: a
    // monotonically-growing vector, never reusing an index, where a
    // std::nullopt entry marks a destroyed slot and an out-of-range index
    // marks one that was never created (see NullPhysicsBackend's own doc
    // comment for the full reasoning - it applies unchanged here). Every
    // BodyId this backend ever hands out therefore has generation 0 for its
    // entire lifetime, so body_state()/destroy_body() never need to check
    // BodyId::generation, for the same structural reason NullPhysicsBackend
    // doesn't. A real body-handle reuse policy remains a later backend's
    // concern (this library's README, "Open questions").
    std::vector<std::optional<JPH::BodyID>> bodies_;

    // The reverse of bodies_ above (issue #180's own first need for one - a
    // raycast/sweep hit only carries Jolt's own JPH::BodyID, and callers must
    // only ever see this backend's own BodyId, never Jolt's). A linear scan
    // over bodies_ per query is fine given this round's scope (this library
    // has never needed more than a handful of bodies in any of its own
    // tests) - see this library's README "Open questions" for why a hash map
    // isn't reached for here without a concrete reason to.
    [[nodiscard]] BodyId body_id_from_jolt(JPH::BodyID jolt_body_id) const noexcept;
};

static_assert(PhysicsBackend<JoltPhysicsBackend>);

} // namespace atlas::physics
