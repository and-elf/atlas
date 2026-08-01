# atlas-docgen links atlas-cgen-lib (see README.md's "Dependency position")
# to reuse atlas::cgen::parse_manifest/Manifest rather than reimplementing
# YAML parsing - tools/CMakeLists.txt adds atlas-cgen before this directory
# so atlas::cgen_lib already exists as a target by the time this runs.
add_library(atlas-docgen-lib src/markdown_writer.cpp)
add_library(atlas::docgen_lib ALIAS atlas-docgen-lib)

target_include_directories(
  atlas-docgen-lib
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-docgen-lib PUBLIC atlas_project_options atlas::cgen_lib
                       PRIVATE atlas_project_warnings)

add_executable(atlas-docgen src/main.cpp)
target_link_libraries(atlas-docgen PRIVATE atlas::docgen_lib atlas_project_warnings)
