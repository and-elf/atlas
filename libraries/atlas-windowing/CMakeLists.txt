# atlas-windowing (issue #174): the shared SDL3 window/context mechanism
# atlas-render's ATLAS_RENDER_BACKEND=SDL3 and atlas-input's
# ATLAS_INPUT_BACKEND=SDL3 both opt into, so a real host wanting both a real
# window and real OS input at once gets exactly one SDL_Window (and one OS
# input-focus target) rather than each library silently creating its own -
# see include/atlas/windowing/sdl3_shared_window.hpp's own doc comment for
# the full rationale.
#
# Deliberately SDL3-specific (this library's only real content is
# Sdl3SharedWindow) - unlike every sibling optional library
# (atlas-render/atlas-input/atlas-audio), there is no meaningful "NULL
# backend" for this one to provide: a host that selects neither
# ATLAS_RENDER_BACKEND=SDL3 nor ATLAS_INPUT_BACKEND=SDL3 has nothing to
# share a window between, so atlas-windowing has nothing to build. The
# atlas::windowing target still always exists (so libraries/CMakeLists.txt
# and consuming CMakeLists.txt never need to conditionally add_subdirectory
# or conditionally target_link_libraries this library at all) - it is just
# an empty INTERFACE library in that case.
set(ATLAS_WINDOWING_NEEDS_SDL3 FALSE)
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3" OR ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  set(ATLAS_WINDOWING_NEEDS_SDL3 TRUE)
endif()

if(ATLAS_WINDOWING_NEEDS_SDL3)
  include(FetchContent)

  # Same cache-forced options and SYSTEM rationale as
  # libraries/atlas-render/CMakeLists.txt's own SDL3 FetchContent_Declare -
  # see that file's comments for why each of these matters, not just
  # tidiness. Declaring the exact same dependency (name, GIT_TAG) here too is
  # intentional and safe: FetchContent_Declare/_MakeAvailable are idempotent,
  # so a build with atlas-render and/or atlas-input also selecting SDL3
  # fetches it exactly once and every library shares the same SDL3 target.
  set(SDL_SHARED
      OFF
      CACHE BOOL "" FORCE)
  set(SDL_STATIC
      ON
      CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY
      OFF
      CACHE BOOL "" FORCE)

  FetchContent_Declare(
    SDL3 SYSTEM
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12)
  FetchContent_MakeAvailable(SDL3)

  foreach(sdl3_target IN ITEMS SDL3-static SDL3 SDL_uclibc)
    if(TARGET ${sdl3_target})
      set_target_properties(${sdl3_target} PROPERTIES CXX_CLANG_TIDY "")
    endif()
  endforeach()

  add_library(atlas-windowing src/sdl3_shared_window.cpp)
  target_link_libraries(
    atlas-windowing
    PUBLIC atlas_project_options SDL3::SDL3-static
    PRIVATE atlas_project_warnings)
  target_include_directories(
    atlas-windowing PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)
else()
  add_library(atlas-windowing INTERFACE)
  target_link_libraries(atlas-windowing INTERFACE atlas_project_options)
  target_include_directories(
    atlas-windowing INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                              $<INSTALL_INTERFACE:include>)
endif()

add_library(atlas::windowing ALIAS atlas-windowing)
