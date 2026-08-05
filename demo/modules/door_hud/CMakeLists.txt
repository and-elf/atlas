# demo-door_hud (issue #234, slimmed by #237): now just the Intent ->
# door::OpenDoor translator - Node-building moved to interactable_hud, which
# is generic across every interactable entity type, not door-specific. Only
# needs atlas::input beyond the fixed set atlas_add_demo_capability() links
# unconditionally (see that function's own EXTRA_LINKS comment in
# demo/CMakeLists.txt), for the IntentId/Intent types this translator reads.
# No CAPABILITY_DEPENDS: this file's own #include "door.capability.hpp"
# only needs door's generated contract header (to construct
# atlas::door::OpenDoor) - demo-contracts (linked unconditionally) already
# gets it that header; door_hud.cpp never calls into door.cpp's own
# implementation, matching health's own precedent for armor (see
# demo/modules/health/CMakeLists.txt's comment).
atlas_add_demo_capability(
  NAME door_hud
  SOURCES door_hud.cpp
  EXTRA_LINKS atlas::input)
