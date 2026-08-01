include(FetchContent)

set(YAML_CPP_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS
    OFF
    CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB
    OFF
    CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL
    OFF
    CACHE BOOL "" FORCE)

# Duplicated from tools/atlas-cgen/CMakeLists.txt verbatim (same GIT_TAG),
# not shared via a common CMake include - FetchContent_Declare/
# _MakeAvailable are idempotent per dependency name, so declaring yaml-cpp a
# second time across sibling tools/ subdirectories does not re-download or
# conflict (the same precedent tools/atlas-rcc/README.md documents for its
# own duplicated declaration). Keeps atlas-refl free of a build-ordering
# dependency on atlas-cgen's own CMakeLists.txt existing or running first -
# see this tool's README, "Dependency position".
#
# SYSTEM (CMake >= 3.25, our minimum) marks yaml-cpp's headers -isystem for
# consumers - without it, a warning that originates purely inside yaml-cpp's
# own headers (e.g. its enum members shadowing each other, -Wshadow) still
# gets attributed to whichever of OUR files #included yaml-cpp and fails our
# -Werror. HeaderFilterRegex in .clang-tidy only suppresses clang-tidy's own
# check diagnostics for third-party headers, not plain compiler diagnostics
# clang-tidy also surfaces - SYSTEM is what actually silences those.
FetchContent_Declare(
  yaml-cpp SYSTEM
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0)
FetchContent_MakeAvailable(yaml-cpp)

# Third-party sources are out of scope for our clang-tidy gate (CMAKE_CXX_CLANG_TIDY
# is a global default applied to every target, including fetched dependencies).
if(TARGET yaml-cpp)
  set_target_properties(yaml-cpp PROPERTIES CXX_CLANG_TIDY "")
endif()

add_library(atlas-refl-lib src/manifest.cpp src/template_engine.cpp src/reflection_writer.cpp)
add_library(atlas::refl_lib ALIAS atlas-refl-lib)

target_include_directories(atlas-refl-lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                  $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-refl-lib PUBLIC atlas_project_options yaml-cpp::yaml-cpp
                       PRIVATE atlas_project_warnings)

add_executable(atlas-refl src/main.cpp)
target_link_libraries(atlas-refl PRIVATE atlas::refl_lib atlas_project_warnings)
