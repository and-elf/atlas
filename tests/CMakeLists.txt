include(FetchContent)

set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_GMOCK
    OFF
    CACHE BOOL "" FORCE)

# SYSTEM (CMake >= 3.25, our minimum): see tools/atlas-cgen/CMakeLists.txt's
# yaml-cpp FetchContent_Declare for why this matters, not just tidiness -
# without it, a warning purely inside a third-party header can still fail
# our -Werror via whichever of our files includes it.
FetchContent_Declare(
  googletest SYSTEM
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
add_subdirectory(atlas-resource)
add_subdirectory(atlas-serialization)
add_subdirectory(atlas-reflection)
add_subdirectory(atlas-request)
add_subdirectory(atlas-scheduler)
add_subdirectory(atlas-replication)
add_subdirectory(atlas-runtime)
add_subdirectory(atlas-audio)
add_subdirectory(atlas-input)
add_subdirectory(atlas-cgen)
add_subdirectory(atlas-rcc)
