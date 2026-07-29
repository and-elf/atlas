add_executable(contract-gen-tests manifest_test.cpp template_engine_test.cpp contract_writer_test.cpp)

target_link_libraries(contract-gen-tests PRIVATE atlas::contract_gen_lib GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(contract-gen-tests)

# Build-time proof that contract-gen's actual output compiles and satisfies
# atlas-contracts's concepts for real - not a ctest case, a normal build
# target. If this stops compiling, the *build* fails, which is the correct
# failure mode for a static_assert (spec §12, Compile-Time Validation).
set(ATLAS_CONTRACT_GEN_HEALTH_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/health.capability.yaml")
set(ATLAS_CONTRACT_GEN_HEALTH_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/health.capability.hpp")

add_custom_command(
  OUTPUT "${ATLAS_CONTRACT_GEN_HEALTH_GENERATED}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND $<TARGET_FILE:contract-gen> "${ATLAS_CONTRACT_GEN_HEALTH_FIXTURE}"
          "${ATLAS_CONTRACT_GEN_HEALTH_GENERATED}"
  DEPENDS contract-gen "${ATLAS_CONTRACT_GEN_HEALTH_FIXTURE}"
  COMMENT "Generating health.capability.hpp from fixture manifest"
  VERBATIM)

add_library(contract-gen-health-compile-check OBJECT compile_check/health_compile_check.cpp
                                                      "${ATLAS_CONTRACT_GEN_HEALTH_GENERATED}")
target_include_directories(contract-gen-health-compile-check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(contract-gen-health-compile-check PRIVATE atlas::contracts atlas::entity
                                                                 atlas_project_warnings)
