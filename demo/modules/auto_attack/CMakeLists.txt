# demo-auto_attack (issue #190): see demo/CMakeLists.txt's own comment for
# the per-capability CMake library rationale, and for why the generated
# contract headers this target needs are attached via target_sources() from
# there instead of here.
add_library(demo-auto_attack auto_attack.cpp)
add_library(demo::auto_attack ALIAS demo-auto_attack)

target_link_libraries(
  demo-auto_attack
  PUBLIC demo-contracts demo::attack_resolution demo::movement demo::interruption atlas::contracts
         atlas::core atlas::entity atlas::resource atlas::runtime atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-auto_attack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
