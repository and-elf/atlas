# demo-rigid_body (issue #188): proves atlas-physics' PhysicsBackend feeding a
# real, depends_on/consumes-ordered capability property - see this module's
# own rigid_body.hpp doc comment, and demo/README.md's "Physics" section for
# the full write-up.
#
# Links atlas::physics directly - not one of atlas_add_demo_capability's own
# default links (atlas::contracts/core/entity/resource/runtime/request/
# serialization) - and selects, at compile time, the concrete PhysicsBackend
# this module instantiates via ATLAS_DEMO_PHYSICS_BACKEND_JOLT, defined here
# from the same ATLAS_PHYSICS_BACKEND CMake option atlas-physics' own
# CMakeLists.txt already exposes (never a second, independent option) -
# mirrors tests/atlas-physics/CMakeLists.txt's own
# ATLAS_PHYSICS_BACKEND-conditional pattern, generalized from "which .cpp
# gets compiled" to "which type this module's own rigid_body::Backend alias
# resolves to" (see rigid_body.hpp). PUBLIC: demo-tests' own
# rigid_body_test.cpp gates its Jolt-only tests on this same macro, so it
# must propagate past this target, the same way demo-rigid_body's other
# public links already do.
atlas_add_demo_capability(NAME rigid_body SOURCES rigid_body.cpp CAPABILITY_DEPENDS movement)
target_link_libraries(demo-rigid_body PUBLIC atlas::physics)
if(ATLAS_PHYSICS_BACKEND STREQUAL "JOLT")
  target_compile_definitions(demo-rigid_body PUBLIC ATLAS_DEMO_PHYSICS_BACKEND_JOLT)
endif()
