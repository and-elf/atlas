# demo-door (issue #190): see demo/CMakeLists.txt's own comment for the
# per-capability CMake library rationale, and for why the generated contract
# header this target needs is attached via target_sources() from there
# instead of here.
add_library(demo-door door.cpp)
add_library(demo::door ALIAS demo-door)

target_link_libraries(
  demo-door
  PUBLIC demo-contracts atlas::contracts atlas::core atlas::entity atlas::resource atlas::runtime
         atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-door PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
