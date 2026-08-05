add_executable(
  atlas-cgen-tests
  manifest_test.cpp
  host_manifest_test.cpp
  dependency_graph_test.cpp
  property_graph_test.cpp
  host_composition_test.cpp
  host_composition_writer_test.cpp
  template_engine_test.cpp
  contract_writer_test.cpp)

target_link_libraries(atlas-cgen-tests PRIVATE atlas::cgen_lib GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-cgen-tests)

# Build-time proof that atlas-cgen's actual output compiles and satisfies
# atlas-contracts's concepts for real - not a ctest case, a normal build
# target. If this stops compiling, the *build* fails, which is the correct
# failure mode for a static_assert (spec §12, Compile-Time Validation).
#
# The fixture manifests live under tests/fixtures/ (shared with
# tests/atlas-contracts, which proves its own concepts against this same
# generator output rather than a hand-copied stand-in struct).
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_cgen_health_generated)

add_library(atlas-cgen-health-compile-check OBJECT compile_check/health_compile_check.cpp
                                                    "${atlas_cgen_health_generated}")
target_include_directories(atlas-cgen-health-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-cgen-health-compile-check PRIVATE atlas::contracts atlas::entity
                                                               atlas_project_warnings)

# Same build-time proof, for a ResourceId-typed field (spec §12, Compile-Time
# Validation) - health.capability.yaml only ever exercised EntityRef, so
# without this, ResourceId support would only ever be checked by string
# assertions on generated text, never by actually compiling the result.
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/cosmetics.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_cgen_cosmetics_generated)

add_library(atlas-cgen-cosmetics-compile-check OBJECT compile_check/cosmetics_compile_check.cpp
                                                       "${atlas_cgen_cosmetics_generated}")
target_include_directories(atlas-cgen-cosmetics-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-cgen-cosmetics-compile-check PRIVATE atlas::contracts atlas::entity
                                                                  atlas::resource atlas_project_warnings)

# Same build-time proof, for a composed (composition: Additive) property
# (spec §12, Compile-Time Validation; §20 Property Composition) - neither
# health.capability.yaml nor cosmetics.capability.yaml exercise the
# composition: key, so without this, atlas::Composable support would only
# ever be checked by string assertions on generated text.
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/armor.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_cgen_armor_generated)

add_library(atlas-cgen-armor-compile-check OBJECT compile_check/armor_compile_check.cpp
                                                   "${atlas_cgen_armor_generated}")
target_include_directories(atlas-cgen-armor-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-cgen-armor-compile-check PRIVATE atlas::contracts atlas::entity
                                                              atlas_project_warnings)

# Same build-time proof, for a SessionId-typed field (spec §12, Compile-Time
# Validation; §6 Session Identity) - issue #223's manifest.cpp mapping-table
# work landed before libraries/atlas-session existed on main, so this
# compile-check step was deferred until #222 (atlas::SessionId itself)
# merged; without it, SessionId support would only ever be checked by
# string assertions on generated text, never by actually compiling the
# result.
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/session_example.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_cgen_session_example_generated)

add_library(atlas-cgen-session-compile-check OBJECT compile_check/session_example_compile_check.cpp
                                                     "${atlas_cgen_session_example_generated}")
target_include_directories(atlas-cgen-session-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-cgen-session-compile-check PRIVATE atlas::contracts atlas::entity atlas::session
                                                                atlas_project_warnings)

# Same build-time proof, for an IntentId-typed field (issue #237's own
# generic Interactable capability needs `action: IntentId`) - mirrors the
# SessionId compile-check above exactly.
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/intent_example.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_cgen_intent_example_generated)

add_library(atlas-cgen-intent-compile-check OBJECT compile_check/intent_example_compile_check.cpp
                                                    "${atlas_cgen_intent_example_generated}")
target_include_directories(atlas-cgen-intent-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-cgen-intent-compile-check PRIVATE atlas::contracts atlas::entity atlas::input
                                                               atlas::resource atlas_project_warnings)

# Build-time proof that atlas-cgen's *host composition* mode (spec §14) -
# resolving gameplay_client.host.yaml's `composes: [health, armor]` into a
# dependency-checked order and emitting a PropertyStore-registration struct
# - actually compiles against real atlas::runtime::PropertyStore/Context and
# both capabilities' own generated contracts, not just against generated
# text. health.capability.yaml/armor.capability.yaml are the exact same
# fixtures/generated headers already produced above - reused rather than
# duplicated, since this proof needs their generated contracts to exist on
# the same include path the host composition header's own #includes expect.
atlas_generate_host_composition(
  HOST_FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/gameplay_client.host.yaml"
  CAPABILITY_FIXTURES "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml"
                      "${CMAKE_SOURCE_DIR}/tests/fixtures/armor.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_cgen_gameplay_client_host_generated)

add_library(
  atlas-cgen-host-composition-compile-check OBJECT compile_check/host_composition_compile_check.cpp
                                                    "${atlas_cgen_gameplay_client_host_generated}"
                                                    "${atlas_cgen_health_generated}" "${atlas_cgen_armor_generated}")
target_include_directories(atlas-cgen-host-composition-compile-check
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-cgen-host-composition-compile-check PRIVATE atlas::contracts atlas::entity
                                                                        atlas::runtime atlas_project_warnings)
