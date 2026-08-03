# demo-pathing (issue #190/#191): see demo/CMakeLists.txt's own
# atlas_add_demo_capability() comment for what this expands to.
atlas_add_demo_capability(NAME pathing SOURCES pathing.cpp CAPABILITY_DEPENDS movement)
