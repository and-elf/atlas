# demo-cast_time_attack (issue #190): see demo/CMakeLists.txt's own comment
# for the per-capability CMake library rationale, and for why the generated
# contract headers this target needs are attached via target_sources() from
# there instead of here. Links demo::haste for CastSpeed - a manifest
# `consumes:` entry (property-flow coupling), not a `depends_on`, but still
# a real header dependency (haste::CastSpeed) this capability's own code
# reads.
add_library(demo-cast_time_attack cast_time_attack.cpp)
add_library(demo::cast_time_attack ALIAS demo-cast_time_attack)

target_link_libraries(
  demo-cast_time_attack
  PUBLIC demo-contracts demo::attack_resolution demo::movement demo::interruption demo::haste
         atlas::contracts atlas::core atlas::entity atlas::resource atlas::runtime atlas::request
         atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-cast_time_attack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
