# demo-attack_resolution (issue #190): see demo/CMakeLists.txt's own comment
# for the per-capability CMake library rationale, and for why the generated
# contract headers this target needs are attached via target_sources() from
# there instead of here.
add_library(demo-attack_resolution attack_resolution.cpp)
add_library(demo::attack_resolution ALIAS demo-attack_resolution)

target_link_libraries(
  demo-attack_resolution
  PUBLIC demo-contracts demo::movement demo::line_of_sight demo::health atlas::contracts atlas::core
         atlas::entity atlas::resource atlas::runtime atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-attack_resolution PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
