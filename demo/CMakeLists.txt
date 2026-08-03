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

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/haste/haste.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_haste_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/cast_time_attack/cast_time_attack.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_cast_time_attack_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/interruption/interruption.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_interruption_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/damage_over_time/damage_over_time.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_damage_over_time_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/door/door.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_door_generated)

# demo-host's own host manifest (issue #70) - mirrors
# demo/tests/simulated_host.host.yaml's composed-capability list exactly.
# Generated here, before demo-capabilities below (rather than down next to
# the demo-host executable target, as issue #70 originally had it), because
# app.cpp/app.hpp (issue #70's App class, PR #185 review feedback) - which
# now lives in demo-capabilities, not the demo-host executable itself -
# includes the generated header directly: the custom command
# atlas_generate_host_composition() defines is only wired into the build
# graph if a target in *this same* CMakeLists.txt's directory references its
# output as a source (see demo/tests/CMakeLists.txt's own comment on this
# exact pitfall), and demo-capabilities (immediately below) is that target
# now.
atlas_generate_host_composition(
  HOST_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/demo_host.host.yaml"
  CAPABILITY_FIXTURES
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/health/health.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/armor/armor.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/equipment/equipment.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/movement/movement.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/pathing/pathing.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/aura/aura.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/line_of_sight/line_of_sight.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/auto_attack/auto_attack.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/attack_resolution/attack_resolution.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/haste/haste.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/cast_time_attack/cast_time_attack.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/interruption/interruption.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/damage_over_time/damage_over_time.capability.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/door/door.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_host_generated)

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
  modules/haste/haste.cpp
  modules/cast_time_attack/cast_time_attack.cpp
  modules/damage_over_time/damage_over_time.cpp
  modules/door/door.cpp
  # host_loop.cpp/app.cpp (issue #70; app.cpp per PR #185 review feedback)
  # are host-composition/bootstrap scaffolding, not capabilities - see each
  # one's own header comment - but live here anyway so both demo-host
  # (main.cpp) and demo-tests (host_loop_test.cpp/app_test.cpp) can link
  # them without a separate library for two small files.
  host_loop.cpp
  app.cpp
  "${demo_health_generated}"
  "${demo_armor_generated}"
  "${demo_equipment_generated}"
  "${demo_movement_generated}"
  "${demo_pathing_generated}"
  "${demo_aura_generated}"
  "${demo_line_of_sight_generated}"
  "${demo_auto_attack_generated}"
  "${demo_attack_resolution_generated}"
  "${demo_haste_generated}"
  "${demo_cast_time_attack_generated}"
  "${demo_interruption_generated}"
  "${demo_damage_over_time_generated}"
  "${demo_door_generated}"
  "${demo_host_generated}")

# PUBLIC: every module's own header (health.hpp, armor.hpp, ...) includes its
# generated contract by bare name, and cross-module headers reference each
# other module-prefixed (e.g. equipment.hpp: #include "armor/armor.hpp") -
# both need these two directories on any consumer's include path too, not
# just this library's own .cpp files. CMAKE_CURRENT_SOURCE_DIR itself (this
# directory) is additionally needed so host_loop.hpp (bare #include, issue
# #70) resolves from both demo-tests and demo-host.
target_include_directories(demo-capabilities PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/modules"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
  demo-capabilities
  PUBLIC atlas::contracts atlas::core atlas::entity atlas::resource atlas::runtime atlas::request
         atlas::serialization
  PRIVATE atlas_project_warnings)

# demo-host (issue #70): a real, running in-process host executable - a thin
# wrapper (main.cpp) around atlas::demo::App (app.hpp/app.cpp, above, part of
# demo-capabilities), which owns the actual host composition/tick-loop/argv/
# signal-handling logic. See main.cpp's/app.hpp's own header comments for
# exactly what it does and does not do yet.
add_executable(demo-host main.cpp)
target_link_libraries(demo-host PRIVATE demo-capabilities atlas_project_warnings)

add_subdirectory(tests)
