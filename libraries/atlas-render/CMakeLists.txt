add_library(atlas-render src/frame_builder.cpp src/mesh_asset.cpp src/texture_asset.cpp)
add_library(atlas::render ALIAS atlas-render)

target_include_directories(atlas-render PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-render
  PUBLIC atlas_project_options atlas::core atlas::entity atlas::resource atlas::runtime
  PRIVATE atlas_project_warnings)

# Backend selection (issue #148): which concrete atlas::render::FrameBackend
# a build compiles in, resolved at configure time (spec §4: no runtime
# discovery) - never a runtime factory/plugin lookup. NullFrameBackend
# (include/atlas/render/null_frame_backend.hpp) is header-only and always
# available regardless of this option; ATLAS_RENDER_BACKEND instead gates a
# *real* backend (issue #69, not yet implemented) being compiled in. Only
# "NULL" exists today - selecting anything else fails the configure step
# with a clear message rather than silently building nothing.
set(ATLAS_RENDER_BACKEND
    "NULL"
    CACHE STRING "atlas-render backend to build: NULL (always available, no window/GPU) \
or a real backend once issue #69 lands")
set_property(CACHE ATLAS_RENDER_BACKEND PROPERTY STRINGS "NULL")

if(NOT ATLAS_RENDER_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_RENDER_BACKEND='${ATLAS_RENDER_BACKEND}' is not implemented yet - only NULL exists "
      "today; see issue #69 for the real backend this option will eventually gate")
endif()
