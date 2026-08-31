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

# SYSTEM (CMake >= 3.25, our minimum): see tools/atlas-rcc/CMakeLists.txt's
# identical FetchContent_Declare for why this matters and why it's
# duplicated here rather than shared - no build-ordering dependency on any
# sibling tool's CMakeLists.txt existing or running first.
FetchContent_Declare(
  yaml-cpp SYSTEM
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0)
FetchContent_MakeAvailable(yaml-cpp)

if(TARGET yaml-cpp)
  set_target_properties(yaml-cpp PROPERTIES CXX_CLANG_TIDY "")
endif()

add_library(atlas-adl-lib src/asset_request.cpp)
add_library(atlas::adl_lib ALIAS atlas-adl-lib)

target_include_directories(atlas-adl-lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                 $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-adl-lib PUBLIC atlas_project_options yaml-cpp::yaml-cpp
                       PRIVATE atlas_project_warnings)

add_executable(atlas-adl src/main.cpp)
target_link_libraries(atlas-adl PRIVATE atlas::adl_lib atlas_project_warnings)
