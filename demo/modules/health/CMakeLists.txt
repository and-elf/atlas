# demo-health (issue #190/#191): see demo/CMakeLists.txt's own
# atlas_add_demo_capability() comment for what this expands to. No
# CAPABILITY_DEPENDS: health.cpp only ever reads armor's composed Armor
# property through its generated contract (ctx.get<armor::Armor>()), never
# armor.hpp/armor.cpp directly (health.hpp's own "mutually decoupled"
# comment) - demo-contracts (linked unconditionally) already gets it that
# header.
atlas_add_demo_capability(NAME health SOURCES health.cpp)
