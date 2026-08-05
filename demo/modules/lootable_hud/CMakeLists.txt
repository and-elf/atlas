# demo-lootable_hud (issue #237): the Intent -> lootable::PickUp translator,
# mirroring demo/modules/door_hud/CMakeLists.txt's own (slimmed) precedent
# exactly. Only needs atlas::input beyond the fixed set
# atlas_add_demo_capability() links unconditionally, for the IntentId/Intent
# types this translator reads. No CAPABILITY_DEPENDS: this file's own
# #include "lootable.capability.hpp" only needs lootable's generated
# contract header - demo-contracts (linked unconditionally) already gets it
# that header; lootable_hud.cpp never calls into lootable.cpp's own
# implementation.
atlas_add_demo_capability(
  NAME lootable_hud
  SOURCES lootable_hud.cpp
  EXTRA_LINKS atlas::input)
