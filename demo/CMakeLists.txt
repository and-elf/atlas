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

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/equipment/equipment.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_equipment_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/movement/movement.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_movement_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/pathing/pathing.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_pathing_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/aura/aura.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_aura_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/line_of_sight/line_of_sight.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_line_of_sight_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/auto_attack/auto_attack.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_auto_attack_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/attack_resolution/attack_resolution.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_attack_resolution_generated)

add_library(
  demo-capabilities
  modules/health/health.cpp
  modules/armor/armor.cpp
  modules/equipment/equipment.cpp
  modules/movement/movement.cpp
  modules/pathing/pathing.cpp
  modules/aura/aura.cpp
  modules/line_of_sight/line_of_sight.cpp
  modules/auto_attack/auto_attack.cpp
  modules/attack_resolution/attack_resolution.cpp
  "${demo_health_generated}"
  "${demo_armor_generated}"
  "${demo_equipment_generated}"
  "${demo_movement_generated}"
  "${demo_pathing_generated}"
  "${demo_aura_generated}"
  "${demo_line_of_sight_generated}"
  "${demo_auto_attack_generated}"
  "${demo_attack_resolution_generated}")

# PUBLIC: every module's own header (health.hpp, armor.hpp, ...) includes its
# generated contract by bare name, and cross-module headers reference each
# other module-prefixed (e.g. equipment.hpp: #include "armor/armor.hpp") -
# both need these two directories on any consumer's include path too, not
# just this library's own .cpp files.
target_include_directories(demo-capabilities PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/modules")

target_link_libraries(
  demo-capabilities
  PUBLIC atlas::contracts atlas::core atlas::entity atlas::resource atlas::runtime atlas::request
         atlas::serialization
  PRIVATE atlas_project_warnings)

add_subdirectory(tests)
