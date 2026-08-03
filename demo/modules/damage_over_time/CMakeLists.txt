# demo-damage_over_time (issue #190/#191): see demo/CMakeLists.txt's own
# atlas_add_demo_capability() comment for what this expands to.
atlas_add_demo_capability(NAME damage_over_time SOURCES damage_over_time.cpp CAPABILITY_DEPENDS health)
