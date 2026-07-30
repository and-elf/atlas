# Host composition (spec §14) for this file's own SimulatedHost - generates
# the PropertyStore-registration struct/function every composed
# capability's properties need, replacing what used to be 11 hand-written
# ctx.register_property_store(...) calls (see simulated_host.host.yaml and
# simulated_host.hpp's own comments). Called here, in the same directory as
# the target that consumes its output, deliberately: a custom command's
# OUTPUT (add_custom_command, inside atlas_generate_host_composition) is
# only wired into the actual build graph if a target in *this same*
# CMakeLists.txt's directory references it as a source - referencing it
# from demo/CMakeLists.txt's own add_executable/add_library one directory
# up is silently dropped, no error, the file simply never gets generated.
atlas_generate_host_composition(
  HOST_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/simulated_host.host.yaml"
  CAPABILITY_FIXTURES
    "${CMAKE_SOURCE_DIR}/demo/modules/health/health.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/armor/armor.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/equipment/equipment.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/movement/movement.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/pathing/pathing.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/aura/aura.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/line_of_sight/line_of_sight.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/auto_attack/auto_attack.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/attack_resolution/attack_resolution.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/haste/haste.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/cast_time_attack/cast_time_attack.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/interruption/interruption.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/damage_over_time/damage_over_time.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_simulated_host_generated)

add_executable(
  demo-tests
  combat_scenario_test.cpp
  equipment_test.cpp
  health_test.cpp
  armor_test.cpp
  movement_test.cpp
  healing_test.cpp
  pathing_test.cpp
  aura_test.cpp
  line_of_sight_test.cpp
  auto_attack_test.cpp
  attack_resolution_test.cpp
  cast_time_attack_test.cpp
  haste_test.cpp
  damage_over_time_test.cpp
  fireball_test.cpp
  "${demo_simulated_host_generated}")

# demo-capabilities already puts its own generated/ directory (the single-
# capability contracts every module .hpp includes) on demo-tests' include
# path transitively (PUBLIC target_include_directories, demo/CMakeLists.txt) -
# this target's own generated/ directory (simulated_host.host.hpp) is a
# separate directory (this file's own CMAKE_CURRENT_BINARY_DIR, not demo/'s),
# so it needs its own explicit entry here.
target_include_directories(demo-tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(demo-tests PRIVATE demo-capabilities GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(demo-tests)
