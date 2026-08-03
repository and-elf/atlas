# demo-health (issue #190): one CMake library per capability, following the
# libraries/<name> pattern - see demo/CMakeLists.txt's own comment for why
# (and why not a generated-CMake-fragment approach instead). The generated
# contract header(s) this target's source actually needs are attached via
# target_sources() from demo/CMakeLists.txt itself, right after this
# directory is added - see that file's own comment for why it has to happen
# there, not here.
add_library(demo-health health.cpp)
add_library(demo::health ALIAS demo-health)

# health.cpp only ever reads armor's composed Armor property through its
# generated contract (ctx.get<armor::Armor>()), never armor.hpp/armor.cpp
# directly (health.hpp's own "mutually decoupled" comment) - so no
# demo::armor link here, only demo-contracts (for the include *path* to
# every generated contract, health's own included).
target_link_libraries(
  demo-health
  PUBLIC demo-contracts atlas::contracts atlas::core atlas::entity atlas::resource atlas::runtime
         atlas::request atlas::serialization
  PRIVATE atlas_project_warnings)

target_include_directories(demo-health PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
