# Shared helper for the "run atlas-cgen's host-composition mode against a
# host manifest plus the capability manifests it composes, at build time"
# pattern (spec §12, Compile-Time Validation; spec §14, host composition).
# Mirrors atlas_generate_capability_contract (GenerateCapabilityContract.cmake)
# for the single-capability case - kept as a separate function rather than
# folded into it, since the underlying atlas-cgen invocation takes a
# genuinely different shape (--host plus a variable-length manifest list,
# not a single manifest in/header out).
#
# Usage:
#   atlas_generate_host_composition(
#     HOST_FIXTURE          "${CMAKE_SOURCE_DIR}/tests/fixtures/gameplay_client.host.yaml"
#     CAPABILITY_FIXTURES    "${CMAKE_SOURCE_DIR}/tests/fixtures/health.capability.yaml" ...
#     OUTPUT_DIR             "${CMAKE_CURRENT_BINARY_DIR}/generated"
#     OUT_HEADER             generated_header)
#   # ${generated_header} now holds the full path to the generated .hpp,
#   # produced by a custom command any target can DEPENDS on by listing the
#   # path among its sources.
function(atlas_generate_host_composition)
  cmake_parse_arguments(ARG "" "HOST_FIXTURE;OUTPUT_DIR;OUT_HEADER" "CAPABILITY_FIXTURES" ${ARGN})

  # Same NAME_WE pitfall atlas_generate_capability_contract documents:
  # "gameplay_client.host.yaml" must become "gameplay_client.host.hpp", not
  # "gameplay_client.hpp" - a plain suffix swap, not the longest-extension
  # stripping get_filename_component(... NAME_WE) would do.
  get_filename_component(fixture_filename "${ARG_HOST_FIXTURE}" NAME)
  string(REGEX REPLACE "\\.yaml$" ".hpp" generated_filename "${fixture_filename}")
  set(generated_header "${ARG_OUTPUT_DIR}/${generated_filename}")

  add_custom_command(
    OUTPUT "${generated_header}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND $<TARGET_FILE:atlas-cgen> --host "${ARG_HOST_FIXTURE}" "${generated_header}"
            ${ARG_CAPABILITY_FIXTURES}
    DEPENDS atlas-cgen "${ARG_HOST_FIXTURE}" ${ARG_CAPABILITY_FIXTURES}
    COMMENT "Generating ${generated_filename} from host manifest"
    VERBATIM)

  set(${ARG_OUT_HEADER}
      "${generated_header}"
      PARENT_SCOPE)
endfunction()
