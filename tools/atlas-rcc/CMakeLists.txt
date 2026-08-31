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

# SYSTEM (CMake >= 3.25, our minimum) marks yaml-cpp's headers -isystem for
# consumers - without it, a warning that originates purely inside yaml-cpp's
# own headers (e.g. its enum members shadowing each other, -Wshadow) still
# gets attributed to whichever of OUR files #included yaml-cpp and fails our
# -Werror. HeaderFilterRegex in .clang-tidy only suppresses clang-tidy's own
# check diagnostics for third-party headers, not the plain compiler diagnostics
# clang-tidy also surfaces - SYSTEM is what actually silences those. Declared
# again here (rather than shared with tools/atlas-cgen) so this tool has no
# build-ordering dependency on atlas-cgen's own CMakeLists.txt existing or
# running first - FetchContent_Declare/_MakeAvailable are idempotent per
# dependency name, so declaring the same yaml-cpp@0.8.0 twice across two
# sibling tools is safe, not a duplicated download.
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

add_library(
  atlas-rcc-lib src/resource_manifest.cpp src/resource_table.cpp src/resource_blob.cpp
               src/host_resource_manifest.cpp src/host_resource_compilation.cpp)
add_library(atlas::rcc_lib ALIAS atlas-rcc-lib)

target_include_directories(atlas-rcc-lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                 $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-rcc-lib PUBLIC atlas_project_options atlas::resource yaml-cpp::yaml-cpp
                       PRIVATE atlas_project_warnings)

add_executable(atlas-rcc src/main.cpp)
target_link_libraries(atlas-rcc PRIVATE atlas::rcc_lib atlas_project_warnings)
