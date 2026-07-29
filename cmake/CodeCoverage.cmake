# Coverage instrumentation (ATLAS_ENABLE_COVERAGE) plus the `coverage`
# build target that renders a report and enforces CLAUDE.md's 75% gate via
# gcovr's --fail-under-line/--fail-under-branch (a non-zero gcovr exit
# code fails the target, and therefore the CI job invoking it).
set(ATLAS_COVERAGE_THRESHOLD
    75
    CACHE STRING "Minimum required line/branch coverage percentage")

function(atlas_enable_coverage target_name)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(WARNING "ATLAS_ENABLE_COVERAGE is ON but coverage is only wired up for GCC/Clang")
    return()
  endif()

  target_compile_options(${target_name} INTERFACE --coverage -O0 -g)
  target_link_options(${target_name} INTERFACE --coverage)
endfunction()

find_program(GCOVR_EXECUTABLE gcovr)
if(GCOVR_EXECUTABLE)
  # NOTE: these must be plain double-quoted CMake strings, not '...' —
  # single quotes are not special to CMake's argument parser and would be
  # passed to gcovr as literal characters, silently turning the exclude
  # into a no-op regex that matches nothing.
  #
  # tools/*/src/main.cpp: CLI entry points are integration-level (argv
  # parsing, file I/O error paths) rather than unit-testable without
  # subprocess-spawning test infrastructure this project doesn't build yet
  # (see tools/atlas-cgen/src/main.cpp's own header comment) — excluded
  # the same way tests/ itself is, not a loophole around the gate.
  set(ATLAS_COVERAGE_EXCLUDES
      --exclude "${CMAKE_SOURCE_DIR}/tests/.*" --exclude "${CMAKE_BINARY_DIR}/.*" --exclude
      "${CMAKE_SOURCE_DIR}/tools/[^/]+/src/main.cpp")

  add_custom_target(
    coverage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND
      ${GCOVR_EXECUTABLE} --root ${CMAKE_SOURCE_DIR} --object-directory ${CMAKE_BINARY_DIR}
      ${ATLAS_COVERAGE_EXCLUDES}
      # Compiler-generated exception-unwind edges (every std::string/
      # std::vector operation gets an implicit "what if this throws
      # bad_alloc" branch) are not testable application logic and
      # shouldn't count against the gate - this is gcovr's own
      # purpose-built option for exactly that well-known C++ coverage
      # artifact, not a loophole: it excludes a specific, named category
      # of branch, not arbitrary code.
      --exclude-throw-branches --fail-under-line ${ATLAS_COVERAGE_THRESHOLD} --fail-under-branch
      ${ATLAS_COVERAGE_THRESHOLD} --print-summary --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
      # Explicit search path, scoped to our own build directory only. Without
      # this, gcovr's default search path is "--root + --object-directory" -
      # i.e. it recursively scans the ENTIRE source root for .gcno/.gcda
      # files, not just this build. Any stray coverage artifacts elsewhere
      # under the repo root (a leftover build/ from another preset, a git
      # worktree with its own build directory, etc.) get silently merged
      # into the report, which can badly skew the gate in either direction.
      ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report (gate: ${ATLAS_COVERAGE_THRESHOLD}% line + branch)"
    VERBATIM)
else()
  message(STATUS "gcovr not found locally — 'coverage' target unavailable (CI installs it)")
endif()
