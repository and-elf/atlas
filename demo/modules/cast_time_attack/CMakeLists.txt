# demo-cast_time_attack (issue #190/#191): see demo/CMakeLists.txt's own
# atlas_add_demo_capability() comment for what this expands to. Depends on
# haste for CastSpeed - a manifest `consumes:` entry (property-flow
# coupling), not a `depends_on`, but still a real header dependency
# (haste::CastSpeed) this capability's own code reads.
atlas_add_demo_capability(
  NAME cast_time_attack
  SOURCES cast_time_attack.cpp
  CAPABILITY_DEPENDS attack_resolution movement interruption haste)
