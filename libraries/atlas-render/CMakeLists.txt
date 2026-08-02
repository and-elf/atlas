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
# *real* backend being compiled in on top of it. "SDL3" (issue #151, the
# first slice of #69) is the first real backend - window + SDL_GPU device
# bring-up, a clear-and-present loop, nothing more (real geometry/shaders
# are #153/#154's job). Selecting anything else fails the configure step
# with a clear message rather than silently building nothing.
set(ATLAS_RENDER_BACKEND
    "NULL"
    CACHE STRING "atlas-render backend to build: NULL (always available, no window/GPU) \
or SDL3 (issue #151 - SDL3/SDL_GPU window+device bring-up)")
set_property(CACHE ATLAS_RENDER_BACKEND PROPERTY STRINGS "NULL" "SDL3")

if(ATLAS_RENDER_BACKEND STREQUAL "SDL3")
  include(FetchContent)

  # Static, no shared lib to keep alongside test/tool binaries at run time -
  # matches this project's existing FetchContent precedent (yaml-cpp,
  # GoogleTest) of linking fetched dependencies statically. SDL_TEST_LIBRARY
  # defaults ON even when SDL3 is consumed as a subproject (SDL_TESTS/
  # SDL_EXAMPLES/SDL_INSTALL already default OFF in that case) - explicitly
  # turned off since this project has no use for SDL's own test harness.
  set(SDL_SHARED
      OFF
      CACHE BOOL "" FORCE)
  set(SDL_STATIC
      ON
      CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY
      OFF
      CACHE BOOL "" FORCE)

  # SYSTEM (CMake >= 3.25, our minimum) - see tools/atlas-cgen/CMakeLists.txt's
  # yaml-cpp FetchContent_Declare for why this matters, not just tidiness:
  # without it, a warning purely inside SDL3's own headers can still fail
  # our -Werror via whichever of our files includes them.
  FetchContent_Declare(
    SDL3 SYSTEM
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12)
  FetchContent_MakeAvailable(SDL3)

  # Third-party sources are out of scope for our clang-tidy gate
  # (CMAKE_CXX_CLANG_TIDY is a global default applied to every target,
  # including fetched dependencies).
  foreach(sdl3_target IN ITEMS SDL3-static SDL3 SDL_uclibc)
    if(TARGET ${sdl3_target})
      set_target_properties(${sdl3_target} PROPERTIES CXX_CLANG_TIDY "")
    endif()
  endforeach()

  target_sources(atlas-render PRIVATE src/sdl3_frame_backend.cpp)
  target_link_libraries(atlas-render PUBLIC SDL3::SDL3-static)
elseif(NOT ATLAS_RENDER_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_RENDER_BACKEND='${ATLAS_RENDER_BACKEND}' is not a supported value - only NULL and "
      "SDL3 exist today")
endif()
