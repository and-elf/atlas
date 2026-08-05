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
    "${CMAKE_SOURCE_DIR}/demo/modules/door/door.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/door_hud/door_hud.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/interactable/interactable.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/interactable_hud/interactable_hud.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/lootable/lootable.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/lootable_hud/lootable_hud.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_simulated_host_generated)

# A second, deliberately minimal host composition (issue #188), alongside
# simulated_host.host.yaml above - CLAUDE.md's "test a capability by
# composing it into a minimal test host" rule: rigid_body_test.cpp needs
# only movement (rigid_body's own initial-Position source)/rigid_body/
# physics_observer, not SimulatedHost's much larger composed set built for
# every other demo capability's own scenarios.
atlas_generate_host_composition(
  HOST_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/physics_host.host.yaml"
  CAPABILITY_FIXTURES
    "${CMAKE_SOURCE_DIR}/demo/modules/movement/movement.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/rigid_body/rigid_body.capability.yaml"
    "${CMAKE_SOURCE_DIR}/demo/modules/physics_observer/physics_observer.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER demo_physics_host_generated)

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
  door_test.cpp
  door_hud_test.cpp
  interactable_hud_test.cpp
  lootable_test.cpp
  lootable_hud_test.cpp
  rigid_body_test.cpp
  host_loop_test.cpp
  app_test.cpp
  input_wiring_test.cpp
  presentation_sync_test.cpp
  presentation_app_test.cpp
  player_resources_test.cpp
  "${demo_simulated_host_generated}"
  "${demo_physics_host_generated}")

# demo-capabilities already puts its own generated/ directory (the single-
# capability contracts every module .hpp includes) on demo-tests' include
# path transitively (PUBLIC target_include_directories, demo/CMakeLists.txt) -
# this target's own generated/ directory (simulated_host.host.hpp) is a
# separate directory (this file's own CMAKE_CURRENT_BINARY_DIR, not demo/'s),
# so it needs its own explicit entry here.
target_include_directories(demo-tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(demo-tests PRIVATE demo-capabilities atlas::replication GTest::gtest_main
                                          atlas_project_warnings)

# issue #197 (#71 part 2): presentation_app_sdl3_test.cpp exercises the real
# Sdl3RawSignalSource/Sdl3FrameBackend/Sdl3SharedWindow composition - only
# buildable (and meaningful) when both backend options actually select
# SDL3, matching demo/CMakeLists.txt's own ATLAS_DEMO_REAL_SDL3_BACKEND
# gating for main.cpp's real-backend branch. Every other configuration
# (the default) never compiles this file at all.
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3" AND ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  target_sources(demo-tests PRIVATE presentation_app_sdl3_test.cpp)
endif()

# issue #201: presentation_app_sdl3_audio_test.cpp exercises the real
# Sdl3RawSignalSource/Sdl3FrameBackend/Sdl3AudioBackend composition (audio is
# a third, independent backend axis from render/input - see
# demo/CMakeLists.txt's own ATLAS_DEMO_REAL_AUDIO_BACKEND comment) - only
# buildable (and meaningful) when all three backend options actually select
# SDL3, mirroring presentation_app_sdl3_test.cpp's own gating above exactly,
# extended by one more AND term.
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3"
   AND ATLAS_INPUT_BACKEND STREQUAL "SDL3"
   AND ATLAS_AUDIO_BACKEND STREQUAL "SDL3")
  target_sources(demo-tests PRIVATE presentation_app_sdl3_audio_test.cpp)
endif()

# issue #182: camera_collision_test.cpp constructs a real JoltPhysicsBackend
# directly (proving resolve_camera_collision, demo/camera_collision.hpp,
# actually clamps against real static geometry) - only buildable when
# ATLAS_PHYSICS_BACKEND=JOLT, since atlas/physics/jolt_physics_backend.hpp
# itself is only compiled/available in that configuration
# (libraries/atlas-physics/CMakeLists.txt). atlas::physics is already
# reachable here transitively (demo-capabilities links demo::rigid_body
# PUBLIC atlas::physics) - no new link line needed, mirroring
# demo/tests/rigid_body_test.cpp's own Jolt-only tests.
if(ATLAS_PHYSICS_BACKEND STREQUAL "JOLT")
  target_sources(demo-tests PRIVATE camera_collision_test.cpp)
endif()

# Real fixture blob(s) on disk (demo/tests/fixtures/), not a mocked filesystem -
# matching CLAUDE.md's "test via a minimal test host, not by mocking behavior"
# rule, and the same convention tests/atlas-resource/fixtures/ already uses.
target_compile_definitions(demo-tests PRIVATE DEMO_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# issue #200: player_resources_test.cpp resolves against the real, checked-in
# demo/resources/Mesh.blob/Texture.blob - unconditional (unlike
# DEMO_RESOURCES_DIR's other use in demo/CMakeLists.txt for main.cpp's real-
# backend branch), since resolving/decoding a blob needs no GPU/window at
# all.
target_compile_definitions(demo-tests PRIVATE DEMO_RESOURCES_DIR="${CMAKE_SOURCE_DIR}/demo/resources")

if(ATLAS_RENDER_BACKEND STREQUAL "SDL3" AND ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  # PresentationAppSdl3Test constructs a real Sdl3FrameBackend, which reaches
  # real shader compilation (Sdl3MeshPipeline) exactly like
  # tests/atlas-render-tests does - the same genuine leak inside the
  # vendored, prebuilt DirectXShaderCompiler shared library documented in
  # tests/atlas-render/lsan_suppressions.txt's own doc comment (issue #155),
  # not anything this project's own code did wrong. Reused directly (not
  # duplicated) since it is the exact same leak signature from the exact
  # same third-party library. See that file's CMakeLists.txt for the
  # VK_ICD_FILENAMES half of this same fix (issue #155's Vulkan-loader-
  # enumeration leak) - mirrored here verbatim.
  set(DEMO_TEST_ENVIRONMENT
      "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tests/atlas-render/lsan_suppressions.txt")
  if(EXISTS "/usr/share/vulkan/icd.d/lvp_icd.json")
    list(APPEND DEMO_TEST_ENVIRONMENT "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json")
  endif()
  gtest_discover_tests(demo-tests PROPERTIES ENVIRONMENT "${DEMO_TEST_ENVIRONMENT}")
else()
  gtest_discover_tests(demo-tests)
endif()
