set(ATLAS_PHYSICS_TEST_SOURCES null_physics_backend_test.cpp)

# JoltPhysicsBackend (issue #178) only exists - and only needs testing - when
# atlas-physics was itself configured to build it
# (ATLAS_PHYSICS_BACKEND=JOLT, libraries/atlas-physics/CMakeLists.txt); the
# default NULL build never sees this file at all. Mirrors
# tests/atlas-render/CMakeLists.txt's own ATLAS_RENDER_BACKEND-conditional
# sources pattern exactly.
if(ATLAS_PHYSICS_BACKEND STREQUAL "JOLT")
  list(APPEND ATLAS_PHYSICS_TEST_SOURCES jolt_physics_backend_test.cpp)
endif()

add_executable(atlas-physics-tests ${ATLAS_PHYSICS_TEST_SOURCES})

target_link_libraries(atlas-physics-tests PRIVATE atlas::physics GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-physics-tests)
