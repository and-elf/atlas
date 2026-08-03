# demo-damage_over_time (issue #190): see demo/CMakeLists.txt's own comment
# for the per-capability CMake library rationale, and for why the generated
# contract headers this target needs are attached via target_sources() from
# there instead of here.
add_library(demo-damage_over_time damage_over_time.cpp)
add_library(demo::damage_over_time ALIAS demo-damage_over_time)

target_link_libraries(
  demo-damage_over_time
  PUBLIC demo-contracts demo::health atlas::contracts atlas::core atlas::entity atlas::resource
         atlas::runtime atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-damage_over_time PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
