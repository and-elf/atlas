# Shared helper for the "run atlas-cgen against a fixture manifest at build
# time, so its static_asserts prove the generator's actual output compiles"
# pattern (spec §12, Compile-Time Validation). Used by both tests/atlas-cgen
# (proving the generator itself against its own fixtures) and
# tests/atlas-contracts (proving atlas-contracts's concepts against real
# generator output, rather than a hand-copied struct that could silently
# drift from what atlas-cgen actually emits) - factored out here once a
# second consumer appeared, rather than duplicated a second time.
#
# Usage:
#   atlas_generate_capability_contract(
#     FIXTURE     "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml"
#     OUTPUT_DIR  "${CMAKE_CURRENT_BINARY_DIR}/generated"
#     OUT_HEADER  generated_header)
#   # ${generated_header} now holds the full path to the generated .hpp,
#   # produced by a custom command any target can DEPENDS on by listing the
#   # path among its sources.
function(atlas_generate_capability_contract)
  cmake_parse_arguments(ARG "" "FIXTURE;OUTPUT_DIR;OUT_HEADER" "" ${ARGN})

  # NAME_WE strips the *longest* extension (everything after the first dot),
  # so "health.capability.yaml" would become "health" - not what we want.
  # A plain suffix swap keeps "health.capability.yaml" as
  # "health.capability.hpp", matching atlas-cgen's own naming convention.
  get_filename_component(fixture_filename "${ARG_FIXTURE}" NAME)
  string(REGEX REPLACE "\\.yaml$" ".hpp" generated_filename "${fixture_filename}")
  set(generated_header "${ARG_OUTPUT_DIR}/${generated_filename}")

  add_custom_command(
    OUTPUT "${generated_header}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND $<TARGET_FILE:atlas-cgen> "${ARG_FIXTURE}" "${generated_header}"
    DEPENDS atlas-cgen "${ARG_FIXTURE}"
    COMMENT "Generating ${generated_filename} from fixture manifest"
    VERBATIM)

  set(${ARG_OUT_HEADER}
      "${generated_header}"
      PARENT_SCOPE)
endfunction()
