# demo-armor (issue #190): see demo/CMakeLists.txt's own comment for the
# per-capability CMake library rationale, and for why the generated contract
# header this target needs is attached via target_sources() from there
# instead of here.
add_library(demo-armor armor.cpp)
add_library(demo::armor ALIAS demo-armor)

target_link_libraries(
  demo-armor
  PUBLIC demo-contracts atlas::contracts atlas::core atlas::entity atlas::resource atlas::runtime
         atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-armor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
