add_library(atlas-physics INTERFACE)
add_library(atlas::physics ALIAS atlas-physics)

target_include_directories(atlas-physics INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                    $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-physics INTERFACE atlas_project_options atlas::core)

# Backend selection (issue #177): which concrete atlas::physics::PhysicsBackend
# a build compiles in, resolved at configure time (spec §4: no runtime
# discovery) - never a runtime factory/plugin lookup, mirroring
# ATLAS_RENDER_BACKEND/ATLAS_AUDIO_BACKEND exactly. NullPhysicsBackend
# (include/atlas/physics/null_physics_backend.hpp) is header-only and always
# available regardless of this option; ATLAS_PHYSICS_BACKEND instead gates a
# *real* backend (issue #178, not yet implemented) being compiled in. Only
# "NULL" exists today - selecting anything else fails the configure step
# with a clear message rather than silently building nothing. The
# if/elseif scaffolding below mirrors atlas-render's own ATLAS_RENDER_BACKEND
# handling in spirit, so #178 only needs to add a new
# `elseif(ATLAS_PHYSICS_BACKEND STREQUAL "JOLT")` branch later rather than
# restructuring this file.
set(ATLAS_PHYSICS_BACKEND
    "NULL"
    CACHE STRING "atlas-physics backend to build: NULL (always available, no rigid-body simulation) \
or a real backend once issue #178 lands")
set_property(CACHE ATLAS_PHYSICS_BACKEND PROPERTY STRINGS "NULL")

if(NOT ATLAS_PHYSICS_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_PHYSICS_BACKEND='${ATLAS_PHYSICS_BACKEND}' is not implemented yet - only NULL exists "
      "today; see issue #178 for the real (Jolt) backend this option will eventually gate")
endif()
