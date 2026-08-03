# demo-interruption (issue #190): header-only (no .cpp - no hand-written
# logic, see interruption.hpp's own comment), so an INTERFACE library rather
# than a compiled one - still a real CMake target other capabilities
# (auto_attack, cast_time_attack) link against for its generated contract.
add_library(demo-interruption INTERFACE)
add_library(demo::interruption ALIAS demo-interruption)

target_link_libraries(
  demo-interruption INTERFACE demo-contracts atlas::contracts atlas::core atlas::entity
                               atlas::resource atlas::runtime atlas::request atlas::serialization)

target_include_directories(demo-interruption INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

# demo-interruption itself has no compile step (INTERFACE) for
# target_sources() to attach a real build-order dependency to - unlike every
# compiled capability library, an INTERFACE library's INTERFACE sources
# don't force generation either (see demo/modules/health/CMakeLists.txt's
# own comment on why demo-contracts alone can't). This is harmless here
# specifically because interruption.capability.hpp is never included
# directly except via interruption.hpp, and every real .cpp that includes
# interruption/interruption.hpp (auto_attack.cpp, cast_time_attack.cpp)
# already lists interruption's generated header explicitly as its own
# PRIVATE source (see those directories' own CMakeLists.txt) - so the
# generation is always forced before it's actually needed regardless.
