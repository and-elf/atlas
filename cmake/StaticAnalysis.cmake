# Wires clang-tidy into the build itself (rather than a separate
# after-the-fact script) by setting CMAKE_CXX_CLANG_TIDY, so every
# compiled translation unit is checked as part of `cmake --build`. This is
# what `cmake --preset clang-tidy && cmake --build --preset clang-tidy`
# gives you locally for a full manual sweep of the whole tree.
#
# The automated gates (.githooks/pre-push, CI's static-analysis job) don't
# use this path — checking every tracked file on every push doesn't scale
# as the codebase grows. They configure with this option explicitly
# overridden OFF (-DATLAS_ENABLE_CLANG_TIDY=OFF) to get a fast plain build
# plus compile_commands.json, then invoke clang-tidy directly against only
# the files that actually changed (cmake/scripts/clang-tidy-changed-files.sh).
# --warnings-as-errors=* (used both here and by that script) makes a
# clang-tidy diagnostic fail the build regardless of the WarningsAsErrors
# setting in .clang-tidy.
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
