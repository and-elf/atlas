# demo-equipment (issue #190): see demo/CMakeLists.txt's own comment for the
# per-capability CMake library rationale, and for why the generated contract
# headers this target needs are attached via target_sources() from there
# instead of here.
add_library(demo-equipment equipment.cpp)
add_library(demo::equipment ALIAS demo-equipment)

target_link_libraries(
  demo-equipment
  PUBLIC demo-contracts demo::armor atlas::contracts atlas::core atlas::entity atlas::resource
         atlas::runtime atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-equipment PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
