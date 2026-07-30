# A growing gameplay demo built on top of Atlas, staged inside this repo for
# now - see README.md for why, and for the scope boundary each capability
# here deliberately stays inside. Structured like the "consuming project"
# shape spec §11 documents for anything built on Atlas (modules/ for
# capabilities, its own tests/), not tests/integration/'s one-off "prove the
# mechanism" shape this started as - this is staged application code, not a
# single proof test, even though it still lives in this repo for now.
#
# Each module's manifest is generated via atlas-cgen exactly like
# tests/atlas-cgen and tests/atlas-contracts already do.

# include(GoogleTest) (for demo/tests/CMakeLists.txt's gtest_discover_tests
# call below) was already run once inside tests/CMakeLists.txt, but the
# module's _GOOGLETEST_DISCOVER_TESTS_SCRIPT variable it sets is a plain
# (non-CACHE) variable scoped to that add_subdirectory(tests) call - it does
# not propagate to demo/, a sibling add_subdirectory of tests/, not a child
# of it. Without re-including it here, gtest_discover_tests silently
# generates a POST_BUILD discovery command with an empty -P script path,
# and CTest reports the target as demo-tests_NOT_BUILT despite the binary
# actually building and running fine.
include(GoogleTest)
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/health/health.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_health_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/armor/armor.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_armor_generated)

add_library(
  demo-capabilities
  modules/health/health.cpp
  modules/armor/armor.cpp
  "${demo_health_generated}"
  "${demo_armor_generated}")

# PUBLIC: every module's own header (health.hpp, armor.hpp, ...) includes its
# generated contract by bare name, and cross-module headers reference each
# other module-prefixed (e.g. equipment.hpp: #include "armor/armor.hpp") -
# both need these two directories on any consumer's include path too, not
# just this library's own .cpp files.
target_include_directories(demo-capabilities PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/modules")

target_link_libraries(
  demo-capabilities
  PUBLIC atlas::contracts atlas::entity atlas::resource atlas::runtime atlas::request
         atlas::serialization
  PRIVATE atlas_project_warnings)

add_subdirectory(tests)
