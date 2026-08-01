add_executable(atlas-refl-tests manifest_test.cpp template_engine_test.cpp reflection_writer_test.cpp)

target_link_libraries(atlas-refl-tests PRIVATE atlas::refl_lib GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-refl-tests)

# Build-time proof that atlas-refl's actual output compiles and is
# genuinely consumable alongside atlas-reflection's runtime primitives -
# not a ctest case, a normal build target, mirroring
# tests/atlas-cgen/CMakeLists.txt's own atlas_generate_capability_contract
# usage. Local to this file rather than a shared cmake/*.cmake helper: this
# round's change footprint is kept strictly inside tools/atlas-refl/ and
# tests/atlas-refl/ (see this tool's README) - factoring this out into a
# shared GenerateReflectionMetadata.cmake, the way
# cmake/GenerateCapabilityContract.cmake was factored out once
# tests/atlas-contracts became a second consumer, is a natural follow-up
# once a second consumer of atlas-refl's output actually exists.
function(atlas_generate_reflection_metadata)
  cmake_parse_arguments(ARG "" "FIXTURE;OUTPUT_DIR;OUT_HEADER" "" ${ARGN})

  get_filename_component(fixture_filename "${ARG_FIXTURE}" NAME)
  string(REGEX REPLACE "\\.yaml$" ".reflection.hpp" generated_filename "${fixture_filename}")
  set(generated_header "${ARG_OUTPUT_DIR}/${generated_filename}")

  add_custom_command(
    OUTPUT "${generated_header}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND $<TARGET_FILE:atlas-refl> "${ARG_FIXTURE}" "${generated_header}"
    DEPENDS atlas-refl "${ARG_FIXTURE}"
    COMMENT "Generating ${generated_filename} from fixture manifest"
    VERBATIM)

  set(${ARG_OUT_HEADER}
      "${generated_header}"
      PARENT_SCOPE)
endfunction()

# health.capability.yaml is the same fixture tools/atlas-cgen's own
# compile_check already proves against (tests/atlas-cgen/CMakeLists.txt) -
# reused here rather than duplicated, so this proof and atlas-cgen's stay
# grounded in the exact same manifest (spec §21's worked example).
atlas_generate_capability_contract(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_refl_health_contract_generated)

atlas_generate_reflection_metadata(
  FIXTURE "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml"
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
  OUT_HEADER atlas_refl_health_reflection_generated)

add_library(
  atlas-refl-health-compile-check OBJECT compile_check/health_reflection_compile_check.cpp
                                          "${atlas_refl_health_contract_generated}"
                                          "${atlas_refl_health_reflection_generated}")
target_include_directories(atlas-refl-health-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(atlas-refl-health-compile-check PRIVATE atlas::contracts atlas::entity atlas::reflection
                                                               atlas::refl_lib atlas_project_warnings)
