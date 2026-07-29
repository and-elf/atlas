include(FetchContent)

set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_GMOCK
    OFF
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2)
FetchContent_MakeAvailable(googletest)

# Third-party sources are out of scope for our clang-tidy gate (CMAKE_CXX_CLANG_TIDY
# is a global default applied to every target, including fetched dependencies).
foreach(third_party_target IN ITEMS gtest gtest_main)
  if(TARGET ${third_party_target})
    set_target_properties(${third_party_target} PROPERTIES CXX_CLANG_TIDY "")
  endif()
endforeach()

include(GoogleTest)

add_subdirectory(atlas-core)
add_subdirectory(atlas-entity)
add_subdirectory(atlas-contracts)
add_subdirectory(atlas-stage)
