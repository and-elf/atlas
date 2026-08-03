# demo-interruption (issue #190/#191): header-only (no .cpp - no
# hand-written logic, see interruption.hpp's own comment) - see
# demo/CMakeLists.txt's own atlas_add_demo_capability() comment for what
# omitting SOURCES expands to (an INTERFACE library).
atlas_add_demo_capability(NAME interruption)
