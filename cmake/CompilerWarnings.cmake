# Centralizes the warning set required by CLAUDE.md so every target opts
# in the same way (`target_link_libraries(<tgt> PRIVATE atlas_project_warnings)`)
# instead of each CMakeLists.txt hand-rolling its own -W flags.
function(atlas_set_project_warnings target_name warnings_as_errors)
  set(CLANG_GCC_WARNINGS
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wold-style-cast
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough
      -Wcast-align
      -Wunused)

  set(MSVC_WARNINGS /W4 /permissive-)

  if(warnings_as_errors)
    list(APPEND CLANG_GCC_WARNINGS -Werror)
    list(APPEND MSVC_WARNINGS /WX)
  endif()

  target_compile_options(
    ${target_name}
    INTERFACE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:${CLANG_GCC_WARNINGS}>
              $<$<CXX_COMPILER_ID:MSVC>:${MSVC_WARNINGS}>)
endfunction()
