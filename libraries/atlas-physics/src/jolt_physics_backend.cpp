#include "atlas/physics/jolt_physics_backend.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>
#include <stdexcept>

// This file's overall structure (allocator/job-system/physics-system
// bring-up, the two-layer broadphase/object-layer setup) mirrors Jolt's own
// official HelloWorld example (github.com/jrouwe/JoltPhysicsHelloWorld's
// Source/HelloWorld.cpp, identical to the HelloWorld/HelloWorld.cpp bundled
// inside the fetched JoltPhysics repository itself) - the boilerplate every
// Jolt integration needs, not invented here.

namespace atlas::physics {

namespace {

// One hardcoded collision shape for every body this backend creates (issue
// #178's own documented scope - see jolt_physics_backend.hpp's own doc
// comment and this library's README "Scoping decisions (Jolt)"):
// BodyCreateInfo has no shape field yet (#179's job), so every body - static
// or dynamic - gets a small sphere regardless of what it would conceptually
// represent.
constexpr float default_body_sphere_radius = 0.5F;

// Jolt's own example values (JoltPhysicsHelloWorld's HelloWorld.cpp) - more
// than generous for this round's tests, which create at most a handful of
// bodies; a real game would size these to its own actual body count.
constexpr JPH::uint max_bodies = 1024;
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

JoltPhysicsBackend::JoltPhysicsBackend()
    : temp_allocator_(temp_allocator_size), job_system_(JPH::cMaxPhysicsJobs) {
    physics_system_.Init(max_bodies,
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

    // The one hardcoded placeholder shape every body gets this round - see
    // this file's own top-of-file doc comment and this library's README.
    JPH::BodyCreationSettings settings(new JPH::SphereShape(default_body_sphere_radius),
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
    physics_system_.Update(delta_seconds, collision_steps, &temp_allocator_, &job_system_);
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

} // namespace atlas::physics
