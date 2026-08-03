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

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/rigid_body/rigid_body.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_rigid_body_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/modules/physics_observer/physics_observer.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_physics_observer_generated)

# demo-host's own host manifest (issue #70) - mirrors
# demo/tests/simulated_host.host.yaml's composed-capability list exactly.
# app.cpp/app.hpp (issue #70's App class, PR #185 review feedback) includes
# the generated header directly, and the custom command
# atlas_generate_host_composition() defines is only wired into the build
# graph if a target in *this same* CMakeLists.txt's directory references its
# output as a source (see demo/tests/CMakeLists.txt's own comment on this
# exact pitfall) - demo-contracts (issue #190, immediately below) is that
# target now.
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

# demo-contracts (issue #190): the shared include *path* to every
# capability's generated contract header (plus the generated
# host-composition header) - one shared location rather than splitting per
# capability the way the *implementation* split below does, since nothing
# currently qualifies these includes by subdirectory (e.g. health.cpp:
# #include "armor.capability.hpp", bare), and changing that would be
# unrelated churn. An INTERFACE library - it only ever propagates the
# include path.
add_library(demo-contracts INTERFACE)
target_include_directories(demo-contracts INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# demo-generated-contracts (issue #190): a real target every generated
# header actually DEPENDS on, existing purely so add_dependencies() below
# can force Ninja to generate all of them before any capability library's
# own compile step starts. Two things were tried and rejected first, both
# confirmed against real failures rather than assumed: (1) listing the
# generated headers as INTERFACE sources of demo-contracts above and relying
# on target_link_libraries(PUBLIC demo-contracts) to propagate ordering -
# an INTERFACE library has no build step of its own to order against, so a
# fresh parallel build can still race and compile a capability before its
# own header exists; (2) target_sources(demo-health PRIVATE
# "${demo_health_generated}") called from demo/CMakeLists.txt (this
# directory) against a target *defined* in a different directory
# (demo/modules/health/) - the GENERATED source-file property is itself
# directory-scoped, so the generated file was never actually treated as a
# real build dependency of demo-health's compile step even though the
# custom command's rule did get emitted. add_dependencies() is a pure
# target-to-target build-order edge, independent of source-file properties
# or which directory declared which target - the one mechanism that
# actually worked once tested.
add_custom_target(
  demo-generated-contracts
  DEPENDS "${demo_health_generated}"
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
          "${demo_rigid_body_generated}"
          "${demo_physics_observer_generated}"
          "${demo_host_generated}")

# atlas_add_demo_capability (PR #191 review feedback): one call per
# capability instead of the hand-written add_library/ALIAS/
# target_link_libraries/target_include_directories block every capability's
# own CMakeLists.txt used to repeat - "write what capabilities you depend on
# (if any) and basically nothing else." NAME is the bare capability name
# (demo/modules/<NAME>/); SOURCES is empty for a header-only capability
# (interruption - an INTERFACE library instead of a compiled one), otherwise
# its own .cpp file(s); CAPABILITY_DEPENDS is the *real* set of other
# capabilities this one's own code actually links against (verified against
# real #include lines - see each capability's own CMakeLists.txt for where
# this differs from depends_on/consumes, e.g. health needs armor's generated
# contract but never links demo::armor). Every capability unconditionally
# gets demo-contracts (the shared generated-contract include path) and
# add_dependencies() onto demo-generated-contracts (so its own compile step
# never races the generation of *any* capability's contract header, not just
# its own - see demo-generated-contracts' own comment above for why this is
# the reliable mechanism) - callers never need to think about either.
#
# Demo-specific (hardcodes the demo- prefix, demo:: alias, demo-contracts/
# demo-generated-contracts target names) rather than a general
# cmake/*.cmake helper every consuming project could reuse - no second real
# consumer exists yet to generalize against (CLAUDE.md: don't design for
# hypothetical future requirements).
function(atlas_add_demo_capability)
  cmake_parse_arguments(ARG "" "NAME" "SOURCES;CAPABILITY_DEPENDS" ${ARGN})

  set(target "demo-${ARG_NAME}")
  set(capability_links "")
  foreach(dep IN LISTS ARG_CAPABILITY_DEPENDS)
    list(APPEND capability_links "demo::${dep}")
  endforeach()

  if(ARG_SOURCES)
    add_library(${target} ${ARG_SOURCES})
    add_library(demo::${ARG_NAME} ALIAS ${target})
    target_link_libraries(
      ${target}
      PUBLIC demo-contracts ${capability_links} atlas::contracts atlas::core atlas::entity
             atlas::resource atlas::runtime atlas::request atlas::serialization
      PRIVATE atlas_project_warnings)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
  else()
    # Header-only (no .cpp - no hand-written logic): an INTERFACE library
    # rather than a compiled one, matching interruption's own precedent.
    add_library(${target} INTERFACE)
    add_library(demo::${ARG_NAME} ALIAS ${target})
    target_link_libraries(
      ${target} INTERFACE demo-contracts ${capability_links} atlas::contracts atlas::core
                           atlas::entity atlas::resource atlas::runtime atlas::request
                           atlas::serialization)
    target_include_directories(${target} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
  endif()

  add_dependencies(${target} demo-generated-contracts)
endfunction()

# One CMake library per capability (issue #190), following the
# libraries/<name> pattern already established for platform libraries -
# replacing the single monolithic demo-capabilities library every
# capability's .cpp used to be hand-listed into. Considered instead having
# atlas-cgen parse the manifest's (currently unparsed, see
# atlas::cgen::Manifest's own doc comment) source: block and generate this
# CMake wiring, rejected in favor of this simpler, zero-new-tooling approach.
# Must come after demo-contracts/demo-generated-contracts/
# atlas_add_demo_capability (each capability's own CMakeLists.txt calls it)
# and before demo-capabilities below (which links every demo::<name> alias
# these define).
add_subdirectory(modules/health)
add_subdirectory(modules/armor)
add_subdirectory(modules/equipment)
add_subdirectory(modules/movement)
add_subdirectory(modules/pathing)
add_subdirectory(modules/aura)
add_subdirectory(modules/line_of_sight)
add_subdirectory(modules/interruption)
add_subdirectory(modules/attack_resolution)
add_subdirectory(modules/haste)
add_subdirectory(modules/auto_attack)
add_subdirectory(modules/cast_time_attack)
add_subdirectory(modules/damage_over_time)
add_subdirectory(modules/door)
add_subdirectory(modules/rigid_body)
add_subdirectory(modules/physics_observer)

# demo-capabilities remains as an umbrella target (issue #190) - its own
# external interface is unchanged, so demo-tests/demo-host keep linking it
# exactly as before. Its own sources are now just the non-capability
# host-composition/bootstrap scaffolding (host_loop.cpp/app.cpp - see each
# one's own header comment); every actual capability now lives in its own
# library above, PUBLICly linked here so this remains a single, complete
# link/include surface for any existing consumer.
add_library(demo-capabilities host_loop.cpp app.cpp input_wiring.cpp presentation_sync.cpp)

target_include_directories(demo-capabilities PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
  demo-capabilities
  PUBLIC demo-contracts
         demo::health
         demo::armor
         demo::equipment
         demo::movement
         demo::pathing
         demo::aura
         demo::line_of_sight
         demo::interruption
         demo::attack_resolution
         demo::haste
         demo::auto_attack
         demo::cast_time_attack
         demo::damage_over_time
         demo::door
         demo::rigid_body
         demo::physics_observer
         atlas::contracts
         atlas::core
         atlas::entity
         atlas::resource
         atlas::runtime
         atlas::request
         atlas::serialization
         # issue #71: input/render/audio's mechanism (PresentationApp,
         # presentation_app.hpp) - each of these four libraries is always
         # buildable regardless of ATLAS_*_BACKEND (only the real SDL3
         # implementation is configure-gated, matching atlas-render/
         # atlas-input/atlas-audio's own CMakeLists.txt precedent).
         # atlas::windowing is an empty INTERFACE library unless
         # ATLAS_RENDER_BACKEND/ATLAS_INPUT_BACKEND select SDL3 (its own
         # CMakeLists.txt) - linked explicitly here (issue #197) rather than
         # relying on atlas::input/atlas::render's own transitive PUBLIC
         # link to it, since main.cpp names atlas::windowing::Sdl3SharedWindow
         # directly.
         atlas::input
         atlas::render
         atlas::audio
         atlas::windowing
  PRIVATE atlas_project_warnings)

# See demo-generated-contracts' own comment above: app.cpp includes
# "demo_host.host.hpp" directly, so demo-capabilities needs the same real
# build-order dependency every capability library gets.
add_dependencies(demo-capabilities demo-generated-contracts)

# demo-host (issue #70): a real, running in-process host executable - a thin
# wrapper (main.cpp) around atlas::demo::App (app.hpp/app.cpp, above, part of
# demo-capabilities), which owns the actual host composition/tick-loop/argv/
# signal-handling logic. See main.cpp's/app.hpp's own header comments for
# exactly what it does and does not do yet.
add_executable(demo-host main.cpp)
target_link_libraries(demo-host PRIVATE demo-capabilities atlas_project_warnings)

# issue #197 (#71 part 2): main.cpp's real-backend branch (Sdl3SharedWindow +
# Sdl3RawSignalSource + Sdl3FrameBackend) is only compiled in when both
# backend options actually select SDL3 - every other configuration (the
# default) takes the unchanged Null branch, matching every other real-vs-
# null backend selection in this codebase (a configure-time choice, never a
# runtime factory/plugin lookup, spec §4).
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3" AND ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  target_compile_definitions(demo-host PRIVATE ATLAS_DEMO_REAL_SDL3_BACKEND)

  # issue #200: DEMO_RESOURCES_DIR points main.cpp's real ResourceRegistry at
  # the checked-in demo/resources/Mesh.blob/Texture.blob - only needed in
  # this same real-backend branch, matching demo-tests' own
  # DEMO_TEST_FIXTURES_DIR precedent (demo/tests/CMakeLists.txt).
  target_compile_definitions(demo-host PRIVATE DEMO_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()

add_subdirectory(tests)
