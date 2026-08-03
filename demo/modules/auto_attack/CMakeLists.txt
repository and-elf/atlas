# demo-auto_attack (issue #190/#191): see demo/CMakeLists.txt's own
# atlas_add_demo_capability() comment for what this expands to.
atlas_add_demo_capability(
  NAME auto_attack
  SOURCES auto_attack.cpp
  CAPABILITY_DEPENDS attack_resolution movement interruption)
