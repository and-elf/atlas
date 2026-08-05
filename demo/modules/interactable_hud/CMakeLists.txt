# demo-interactable_hud (issue #237): builds atlas::ui::Node/
# atlas::ui::Clickable generically for any interactable entity, so - like
# door_hud before it - needs atlas::ui/atlas::input beyond the fixed set
# atlas_add_demo_capability() links unconditionally (see that function's own
# EXTRA_LINKS comment in demo/CMakeLists.txt). No CAPABILITY_DEPENDS: this
# file's own #include "interactable.capability.hpp" only needs
# interactable's generated contract header (to read Interactable) -
# demo-contracts (linked unconditionally) already gets it that header;
# interactable_hud.cpp never calls into interactable's implementation
# (interactable has none - it's header-only, see its own CMakeLists.txt).
atlas_add_demo_capability(
  NAME interactable_hud
  SOURCES interactable_hud.cpp
  EXTRA_LINKS atlas::ui atlas::input)
