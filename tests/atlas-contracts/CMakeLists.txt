# contract_concepts_test.cpp exercises atlas-contracts's concepts against
# atlas-cgen's actual generated output for the health capability (spec §21
# worked example) rather than a hand-copied stand-in struct that could
# silently drift from what the generator really emits - same fixture
# tests/atlas-cgen uses to prove the generator itself.
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_contracts_health_generated)

add_executable(atlas-contracts-tests contract_concepts_test.cpp contract_version_test.cpp composition_test.cpp
                                      "${atlas_contracts_health_generated}")

target_include_directories(atlas-contracts-tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-contracts-tests PRIVATE atlas::contracts atlas::entity GTest::gtest_main
                                                     atlas_project_warnings)

gtest_discover_tests(atlas-contracts-tests)
