#include "atlas/physics/jolt_physics_backend.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

// This file's overall structure (allocator/job-system/physics-system
// bring-up, the two-layer broadphase/object-layer setup) mirrors Jolt's own
// official HelloWorld example (github.com/jrouwe/JoltPhysicsHelloWorld's
// Source/HelloWorld.cpp, identical to the HelloWorld/HelloWorld.cpp bundled
// inside the fetched JoltPhysics repository itself) - the boilerplate every
// Jolt integration needs, not invented here.

namespace atlas::physics {

namespace {

// max_bodies (JPH::PhysicsSystem::Init()'s own inMaxBodies) is caller-
// configurable since issue #193 (JoltPhysicsBackendConfig::max_bodies,
// jolt_physics_backend.hpp) - no longer a compile-time constant here.
//
// max_body_pairs/max_contact_constraints remain Jolt's own example values
// (JoltPhysicsHelloWorld's HelloWorld.cpp), fixed rather than caller-
// configurable - see JoltPhysicsBackendConfig::max_bodies's own doc comment
// (jolt_physics_backend.hpp) for why: both size contact-tracking budgets
// that depend on scene contact density, not on body count, so scaling them
// together with max_bodies would not actually track the real constraint
// these two numbers exist to bound.
constexpr JPH::uint num_body_mutexes = 0; // 0 = Jolt's own default mutex count.
constexpr JPH::uint max_body_pairs = 1024;
constexpr JPH::uint max_contact_constraints = 1024;

// 10 MB, matching HelloWorld's own comment: "way too much for this example
// but a typical value" for a JPH::TempAllocatorImpl's pre-allocated
// per-update scratch budget. The explicit std::size_t{10} avoids an implicit
// int-to-size_t widening of the multiplication itself (bugprone-implicit-
// widening-of-multiplication-result).
constexpr std::size_t temp_allocator_size = std::size_t{10} * 1024 * 1024;

// The two object layers this round's minimal setup needs - mirrors
// JoltPhysicsHelloWorld's own `Layers` namespace exactly. A more elaborate
// game would have many more; nothing in this round's scope needs a second
// one beyond "static" vs "dynamic".
namespace layers {
constexpr JPH::ObjectLayer non_moving = 0;
constexpr JPH::ObjectLayer moving = 1;
} // namespace layers

// Mirrors JoltPhysicsHelloWorld's own `BroadPhaseLayers` namespace: one
// broadphase layer per object layer.
namespace broad_phase_layers {
const JPH::BroadPhaseLayer non_moving(0);
const JPH::BroadPhaseLayer moving(1);
} // namespace broad_phase_layers

// Jolt's own global registration (JPH::RegisterDefaultAllocator(),
// JPH::Factory::sInstance, JPH::RegisterTypes()) is process-wide state with
// no reference count of its own - JPH::RegisterTypes() populates a global
// collision-dispatch table (JPH::CollisionDispatch::sInit() and each shape
// type's own sRegister()) that every live JoltPhysicsBackend instance in this
// process depends on. Guard it with a function-local static so it runs
// exactly once no matter how many JoltPhysicsBackend instances this process
// constructs (this library's own test binary constructs a fresh instance per
// TEST()), and deliberately never call JPH::UnregisterTypes() / delete
// JPH::Factory::sInstance anywhere: doing so from one instance's destructor
// while another instance might still be alive and simulating would silently
// break that other instance's collision detection. The one-time, bounded
// cost (a single JPH::Factory, kept alive for the process's entire lifetime)
// is a deliberate trade-off documented here and in this library's README,
// not an oversight - this mirrors how Jolt's own single-process examples
// (HelloWorld, the Samples application) also perform this registration
// exactly once, at process startup, and only ever unregister once, at
// process exit.
void ensure_global_jolt_init() {
    static const bool initialized = [] {
        JPH::RegisterDefaultAllocator();
        // Jolt's own API is a raw, process-lifetime static pointer with no
        // ownership-transfer alternative - this is the one, intentional
        // exception to this project's usual RAII/smart-pointer discipline,
        // matching Jolt's own HelloWorld example exactly, and deliberately
        // never deleted (see this function's own doc comment above).
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        return true;
    }();
    (void)initialized;
}

// --- Threading mode (issue #193) ---------------------------------------------
//
// Builds this instance's own JPH::JobSystem from its JoltPhysicsBackendConfig
// (jolt_physics_backend.hpp) - the only place either concrete job-system type
// (JPH::JobSystemSingleThreaded / JPH::JobSystemThreadPool) is named anywhere
// in this library; JoltPhysicsBackend itself only ever holds the
// std::unique_ptr<JPH::JobSystem> base-class pointer this function returns
// (see the header's own top-of-file doc comment for why that pointer is
// safe to hold and destroy through).
//
// - SingleThreaded: JPH::JobSystemSingleThreaded(inMaxJobs) - #178's own
//   original construction exactly (JPH::cMaxPhysicsJobs, matching
//   JoltPhysicsHelloWorld's own example value), unchanged.
// - ThreadPool: JPH::JobSystemThreadPool(inMaxJobs, inMaxBarriers,
//   inNumThreads) - genuinely a different constructor shape from
//   JobSystemSingleThreaded's, not assumed to mirror it (verified against
//   the real fetched Jolt source, Jolt/Core/JobSystemThreadPool.h): it also
//   needs inMaxBarriers (JPH::cMaxPhysicsBarriers, Jolt's own
//   JoltPhysicsHelloWorld example value for this parameter too) and
//   inNumThreads (this instance's own config.thread_count, itself defaulting
//   to Jolt's own "-1 = auto-detect CPU count" sentinel - see
//   JoltPhysicsBackendConfig::thread_count's own doc comment).
//
// See this library's README, "Determinism investigation (issue #193)", for
// why choosing ThreadPool here does not compromise this backend's own §4
// bit-exact determinism guarantee for anything this backend actually uses
// (step()'s simulation result, and raycast()/sweep()'s single-closest-hit
// queries) - proven by this file's own test suite
// (ThreadPoolModeIdenticalSetupAndStepsProduceBitExactIdenticalState,
// jolt_physics_backend_test.cpp).
std::unique_ptr<JPH::JobSystem> make_job_system(const JoltPhysicsBackendConfig& config) {
    if (config.threading_mode == ThreadingMode::SingleThreaded) {
        return std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    }
    return std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, config.thread_count);
}

// Converts an atlas::physics::BodyShape (body.hpp's own backend-agnostic
// shape variant - box/sphere/capsule/convex hull, issue #179) into a real,
// Jolt-owned collision shape of the matching kind, via std::visit (one branch
// per alternative, per this issue's own explicit instruction). Returns a
// JPH::Ref<JPH::Shape> - Jolt's own intrusive-refcounting smart pointer
// (Jolt/Core/Reference.h) - rather than a raw JPH::Shape* so the caller never
// has to reason about exactly when the returned shape becomes ref-owned:
//
// - BoxShape/SphereShape/CapsuleShape: `new` a fresh JPH::BoxShape/
//   JPH::SphereShape/JPH::CapsuleShape directly, matching those types' own
//   plain-data constructors exactly (JPH::BoxShape takes a half-extent
//   JPH::Vec3, JPH::SphereShape a radius, JPH::CapsuleShape a half-height and
//   a radius) - the freshly constructed object starts at refcount 0 and is
//   immediately adopted by the returned Ref, exactly mirroring how issue
//   #178's own code handed `new JPH::SphereShape(...)` straight to
//   JPH::BodyCreationSettings's raw-pointer constructor with no intervening
//   owner.
// - ConvexHullShape: unlike the three shapes above, JPH::ConvexHullShape has
//   no direct plain-data constructor (verified against the real fetched Jolt
//   source, Jolt/Physics/Collision/Shape/ConvexHullShape.h, rather than
//   assumed from the other three shapes' pattern) - building a hull can fail
//   (too few points, or points too degenerate/collinear to form one), so
//   Jolt's own API is settings-then-Create(): construct a
//   JPH::ConvexHullShapeSettings from the raw point list, call Create(), and
//   check its JPH::Shape::ShapeResult for an error before trusting it holds a
//   shape. A failure here is reported by throwing std::runtime_error, the
//   same convention this backend's create_body() already uses for its own
//   body-budget-exhausted failure (see this file's own create_body()).
JPH::Ref<JPH::Shape> make_jolt_shape(const BodyShape& shape) {
    return std::visit(
        [](const auto& concrete_shape) -> JPH::Ref<JPH::Shape> {
            using ShapeT = std::decay_t<decltype(concrete_shape)>;
            if constexpr (std::is_same_v<ShapeT, BoxShape>) {
                return JPH::Ref<JPH::Shape>(new JPH::BoxShape(JPH::Vec3(concrete_shape.half_extents.x,
                                                                        concrete_shape.half_extents.y,
                                                                        concrete_shape.half_extents.z)));
            } else if constexpr (std::is_same_v<ShapeT, SphereShape>) {
                return JPH::Ref<JPH::Shape>(new JPH::SphereShape(concrete_shape.radius));
            } else if constexpr (std::is_same_v<ShapeT, CapsuleShape>) {
                return JPH::Ref<JPH::Shape>(
                    new JPH::CapsuleShape(concrete_shape.half_height, concrete_shape.radius));
            } else {
                static_assert(std::is_same_v<ShapeT, ConvexHullShape>);

                std::vector<JPH::Vec3> jolt_points;
                jolt_points.reserve(concrete_shape.points.size());
                for (const core::Vec3& point : concrete_shape.points) {
                    jolt_points.emplace_back(point.x, point.y, point.z);
                }

                const JPH::ConvexHullShapeSettings hull_settings(jolt_points.data(),
                                                                 static_cast<int>(jolt_points.size()));
                const JPH::Shape::ShapeResult hull_result = hull_settings.Create();
                if (hull_result.HasError()) {
                    const JPH::String& error = hull_result.GetError();
                    throw std::runtime_error(
                        "JoltPhysicsBackend::create_body: ConvexHullShapeSettings::Create failed: " +
                        std::string(error.data(), error.size()));
                }
                return hull_result.Get();
            }
        },
        shape);
}

} // namespace

JoltPhysicsBackend::BroadPhaseLayerInterfaceImpl::BroadPhaseLayerInterfaceImpl() {
    object_to_broad_phase_[layers::non_moving] = broad_phase_layers::non_moving;
    object_to_broad_phase_[layers::moving] = broad_phase_layers::moving;
}

JPH::uint JoltPhysicsBackend::BroadPhaseLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
    return static_cast<JPH::uint>(object_to_broad_phase_.size());
}

JPH::BroadPhaseLayer
JoltPhysicsBackend::BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
    return object_to_broad_phase_.at(layer);
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char*
JoltPhysicsBackend::BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const {
    if (layer == broad_phase_layers::non_moving) {
        return "NON_MOVING";
    }
    if (layer == broad_phase_layers::moving) {
        return "MOVING";
    }
    return "INVALID";
}
#endif

bool JoltPhysicsBackend::ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer layer1,
                                                                          JPH::BroadPhaseLayer layer2) const {
    switch (layer1) {
        case layers::non_moving:
            return layer2 == broad_phase_layers::moving;
        case layers::moving:
            return true;
        default:
            return false;
    }
}

bool JoltPhysicsBackend::ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer object1,
                                                                  JPH::ObjectLayer object2) const {
    switch (object1) {
        case layers::non_moving:
            return object2 == layers::moving;
        case layers::moving:
            return true;
        default:
            return false;
    }
}

JoltPhysicsBackend::GlobalJoltInit::GlobalJoltInit() {
    ensure_global_jolt_init();
}

JoltPhysicsBackend::JoltPhysicsBackend(JoltPhysicsBackendConfig config)
    : temp_allocator_(temp_allocator_size), job_system_(make_job_system(config)) {
    physics_system_.Init(config.max_bodies,
                         num_body_mutexes,
                         max_body_pairs,
                         max_contact_constraints,
                         broad_phase_layer_interface_,
                         object_vs_broad_phase_layer_filter_,
                         object_layer_pair_filter_);
}

JoltPhysicsBackend::~JoltPhysicsBackend() {
    // Explicit teardown order (mirrors Sdl3FrameBackend::destroy()'s own
    // discipline): every body this instance ever added to the world is
    // removed and destroyed before physics_system_ itself is torn down by
    // its own destructor below. destroy_body() already leaves a destroyed
    // slot as std::nullopt, so this only visits bodies still live at
    // destruction time.
    JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();
    for (std::optional<JPH::BodyID>& body : bodies_) {
        if (body.has_value()) {
            body_interface.RemoveBody(*body);
            body_interface.DestroyBody(*body);
            body.reset();
        }
    }
}

BodyId JoltPhysicsBackend::create_body(const BodyCreateInfo& create_info) {
    JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();

    const JPH::Vec3 position(create_info.position.x, create_info.position.y, create_info.position.z);
    const JPH::Quat rotation(
        create_info.rotation.x, create_info.rotation.y, create_info.rotation.z, create_info.rotation.w);

    const bool is_dynamic = create_info.motion_type == BodyMotionType::Dynamic;

    // create_info.shape (issue #179) converted into a real Jolt shape of the
    // matching kind - see make_jolt_shape()'s own doc comment. Held in a
    // local JPH::Ref for the remainder of this function so the shape stays
    // ref-owned across the JPH::BodyCreationSettings construction below
    // (whose own raw-pointer constructor takes a fresh reference of its own).
    const JPH::Ref<JPH::Shape> shape = make_jolt_shape(create_info.shape);

    JPH::BodyCreationSettings settings(shape.GetPtr(),
                                       position,
                                       rotation,
                                       is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                       is_dynamic ? layers::moving : layers::non_moving);

    const JPH::BodyID body_id = body_interface.CreateAndAddBody(
        settings, is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if (body_id.IsInvalid()) {
        throw std::runtime_error(
            "JoltPhysicsBackend::create_body: JPH::BodyInterface::CreateAndAddBody failed - this "
            "instance's body budget (max_bodies) is exhausted");
    }

    const auto index = static_cast<BodyId::IndexType>(bodies_.size());
    bodies_.emplace_back(body_id);
    return BodyId{.index = index, .generation = 0};
}

void JoltPhysicsBackend::destroy_body(BodyId body) noexcept {
    if (body.index >= bodies_.size()) {
        return;
    }

    std::optional<JPH::BodyID>& stored = bodies_[body.index];
    if (!stored.has_value()) {
        return;
    }

    JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();
    const JPH::BodyID id = *stored;
    body_interface.RemoveBody(id);
    body_interface.DestroyBody(id);
    stored.reset();
}

void JoltPhysicsBackend::step(float delta_seconds) {
    // A single collision step per call, matching HelloWorld's own comment:
    // callers taking a fixed 1/60s-scale timestep (this contract's own
    // documented discipline, physics_backend.hpp) never need more than one.
    constexpr int collision_steps = 1;
    physics_system_.Update(delta_seconds, collision_steps, &temp_allocator_, job_system_.get());
}

std::optional<BodyState> JoltPhysicsBackend::body_state(BodyId body) const noexcept {
    if (body.index >= bodies_.size()) {
        return std::nullopt;
    }

    const std::optional<JPH::BodyID>& stored = bodies_[body.index];
    if (!stored.has_value()) {
        return std::nullopt;
    }

    const JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();
    JPH::RVec3 position = JPH::RVec3::sZero();
    JPH::Quat rotation = JPH::Quat::sIdentity();
    body_interface.GetPositionAndRotation(*stored, position, rotation);

    return BodyState{
        .position = {.x = position.GetX(), .y = position.GetY(), .z = position.GetZ()},
        .rotation = {.x = rotation.GetX(), .y = rotation.GetY(), .z = rotation.GetZ(), .w = rotation.GetW()},
    };
}

BodyId JoltPhysicsBackend::body_id_from_jolt(JPH::BodyID jolt_body_id) const noexcept {
    for (std::size_t index = 0; index < bodies_.size(); ++index) {
        const std::optional<JPH::BodyID>& stored = bodies_[index];
        if (stored.has_value() && *stored == jolt_body_id) {
            return BodyId{.index = static_cast<BodyId::IndexType>(index), .generation = 0};
        }
    }
    // A hit against a body this instance never created/tracks itself (should
    // not happen in practice - a query only ever reports a hit against a
    // body actually present in this instance's own physics_system_ - but
    // there is no sane BodyId to fabricate here, so the null sentinel is the
    // honest answer rather than an assert/throw for a case that can't be
    // triggered from this contract's own public surface).
    return BodyId{};
}

// --- Raycast query (issue #180) ----------------------------------------------
//
// Uses JPH::NarrowPhaseQuery::CastRay's simple closest-hit overload (no
// collector needed - unlike CastShape, verified against the real Jolt
// source, Jolt/Physics/Collision/NarrowPhaseQuery.h, that this overload
// exists and is the recommended entry point for "just the closest hit",
// mirrored by Jolt's own SamplesApp.cpp EProbeMode::Ray case). JPH::RRayCast's
// own documented convention (Jolt/Physics/Collision/RayCast.h) is that
// mDirection's *length* is the ray's max distance, not a separate parameter -
// direction is defensively normalized here (see physics_backend.hpp's own
// doc comment) so this backend's raycast() always reaches exactly
// max_distance regardless of the magnitude of the caller-supplied direction.
std::optional<HitResult>
JoltPhysicsBackend::raycast(core::Vec3 origin, core::Vec3 direction, float max_distance) const {
    const float direction_length =
        std::sqrt((direction.x * direction.x) + (direction.y * direction.y) + (direction.z * direction.z));
    if (!(direction_length > 0.0F) || !(max_distance > 0.0F)) {
        return std::nullopt;
    }

    const float inverse_length = 1.0F / direction_length;
    const JPH::Vec3 jolt_direction(direction.x * inverse_length * max_distance,
                                   direction.y * inverse_length * max_distance,
                                   direction.z * inverse_length * max_distance);
    const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), jolt_direction);

    JPH::RayCastResult hit;
    const bool had_hit = physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit);
    if (!had_hit) {
        return std::nullopt;
    }

    const JPH::RVec3 hit_point = ray.GetPointOnRay(hit.mFraction);

    // The recommended way to get a ray hit's surface normal (Jolt's own
    // NarrowPhaseQuery::CastRay doc comment, and SamplesApp.cpp's
    // EProbeMode::Ray case): lock the hit body and call its own
    // GetWorldSpaceSurfaceNormal(). A lock failure (the body was
    // destroyed concurrently - cannot happen in this single-threaded
    // backend, but BodyLockRead's own contract requires checking
    // Succeeded() regardless) falls back to a zero normal rather than
    // reading through a null Body*.
    JPH::Vec3 normal = JPH::Vec3::sZero();
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), hit.mBodyID);
    if (lock.Succeeded()) {
        normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hit_point);
    }

    return HitResult{
        .body = body_id_from_jolt(hit.mBodyID),
        .point = {.x = hit_point.GetX(), .y = hit_point.GetY(), .z = hit_point.GetZ()},
        .normal = {.x = normal.GetX(), .y = normal.GetY(), .z = normal.GetZ()},
    };
}

// --- Sweep query (issue #180) -------------------------------------------------
//
// Unlike CastRay, JPH::NarrowPhaseQuery::CastShape has no simple
// closest-hit overload (verified against the real Jolt source) - Jolt's own
// idiomatic way to get "just the closest hit" from it is
// JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> (mirrored by
// several of Jolt's own Samples/Tests, e.g. MotorcycleTest.cpp's gravity
// probe and ShapeFilterTest.cpp). RShapeCast::sFromWorldTransform() is
// Jolt's own documented, recommended way to build an RShapeCast from a
// world-space starting transform (rather than hand-assembling its
// center-of-mass-space fields) - used here exactly as documented. `shape` is
// converted via make_jolt_shape() (this file's own anonymous namespace,
// above), the same conversion create_body() already uses, so a sweep tests
// against exactly the same real Jolt shape a body of that BodyShape would
// have.
std::optional<HitResult> JoltPhysicsBackend::sweep(const BodyShape& shape,
                                                   core::Vec3 from_position,
                                                   core::Quaternion from_rotation,
                                                   core::Vec3 to_position) const {
    const JPH::Ref<JPH::Shape> jolt_shape = make_jolt_shape(shape);

    const JPH::Vec3 direction(
        to_position.x - from_position.x, to_position.y - from_position.y, to_position.z - from_position.z);
    const JPH::RMat44 world_transform = JPH::RMat44::sRotationTranslation(
        JPH::Quat(from_rotation.x, from_rotation.y, from_rotation.z, from_rotation.w),
        JPH::RVec3(from_position.x, from_position.y, from_position.z));

    const JPH::RShapeCast shape_cast = JPH::RShapeCast::sFromWorldTransform(
        jolt_shape.GetPtr(), JPH::Vec3::sReplicate(1.0F), world_transform, direction);
    const JPH::ShapeCastSettings settings;

    // Results are reported relative to this offset rather than always in raw
    // world position - Jolt's own doc comment on CastShape recommends
    // picking a position close to the cast itself (e.g. its own start
    // translation, used here) for better floating-point precision "when
    // you're testing far from the origin"; the world-space point is
    // recovered below by adding it back.
    const JPH::RVec3 base_offset = shape_cast.mCenterOfMassStart.GetTranslation();

    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    physics_system_.GetNarrowPhaseQuery().CastShape(shape_cast, settings, base_offset, collector);

    if (!collector.HadHit()) {
        return std::nullopt;
    }

    const JPH::ShapeCastResult& hit = collector.mHit;
    const JPH::RVec3 hit_point = base_offset + hit.mContactPointOn2;

    // CollideShapeResult::mPenetrationAxis points from the swept shape
    // toward the hit body, along the shortest separating direction (Jolt's
    // own doc comment, CollideShape.h) - negating and normalizing it
    // recovers the hit body's own outward surface normal, matching
    // HitResult::normal's documented convention (body.hpp) and Jolt's own
    // recommended usage (CollideShapeResult's doc comment: "you can use
    // -mPenetrationAxis.Normalized() as contact normal").
    const JPH::Vec3 normal = (-hit.mPenetrationAxis).Normalized();

    return HitResult{
        .body = body_id_from_jolt(hit.mBodyID2),
        .point = {.x = hit_point.GetX(), .y = hit_point.GetY(), .z = hit_point.GetZ()},
        .normal = {.x = normal.GetX(), .y = normal.GetY(), .z = normal.GetZ()},
    };
}

} // namespace atlas::physics
