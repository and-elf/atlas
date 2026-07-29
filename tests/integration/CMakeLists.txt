# True integration test (see combat_scenario_test.cpp's header comment):
# composes the health and armor capabilities, generated here via atlas-cgen
# exactly like tests/atlas-cgen and tests/atlas-contracts already do, into a
# hand-composed 3-host scenario proving request dispatch, property
# composition, and replication actually work together end to end.
#
# health's own manifest lives under this directory's fixtures/ (it declares
# depends_on: [entity, armor], specific to this scenario) rather than reusing
# the shared tests/fixtures/health.capability.yaml other suites use, so this
# test's dependency declaration doesn't leak into unrelated fixtures. armor's
# manifest has no such scenario-specific need, so it reuses the shared one.
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/health.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_integration_health_generated)

atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/armor.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_integration_armor_generated)

add_executable(
  atlas-integration-tests
  combat_scenario_test.cpp
  capabilities/health.cpp
  capabilities/armor.cpp
  "${atlas_integration_health_generated}"
  "${atlas_integration_armor_generated}")

target_include_directories(atlas-integration-tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(
  atlas-integration-tests
  PRIVATE atlas::contracts
          atlas::entity
          atlas::runtime
          atlas::request
          atlas::serialization
          GTest::gtest_main
          atlas_project_warnings)

gtest_discover_tests(atlas-integration-tests)
