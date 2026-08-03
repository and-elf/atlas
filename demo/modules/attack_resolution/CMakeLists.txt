# demo-attack_resolution (issue #190/#191): see demo/CMakeLists.txt's own
# atlas_add_demo_capability() comment for what this expands to.
atlas_add_demo_capability(
  NAME attack_resolution
  SOURCES attack_resolution.cpp
  CAPABILITY_DEPENDS movement line_of_sight health)
