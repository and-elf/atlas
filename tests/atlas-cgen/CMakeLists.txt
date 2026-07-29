add_executable(atlas-cgen-tests manifest_test.cpp template_engine_test.cpp contract_writer_test.cpp)

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
