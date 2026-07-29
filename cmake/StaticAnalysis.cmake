# Wires clang-tidy into the build itself (rather than a separate
# after-the-fact script) by setting CMAKE_CXX_CLANG_TIDY, so every
# compiled translation unit is checked as part of `cmake --build`.
# --warnings-as-errors=* makes a clang-tidy diagnostic fail the build
# regardless of the WarningsAsErrors setting in .clang-tidy.
function(atlas_enable_clang_tidy)
  find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
  if(NOT CLANG_TIDY_EXECUTABLE)
    message(WARNING "ATLAS_ENABLE_CLANG_TIDY is ON but clang-tidy was not found on PATH")
    return()
  endif()

  set(CMAKE_CXX_CLANG_TIDY
      "${CLANG_TIDY_EXECUTABLE};--warnings-as-errors=*"
      PARENT_SCOPE)
endfunction()
