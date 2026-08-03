add_library(atlas-input src/scripted_raw_signal_source.cpp src/intent_router.cpp)
add_library(atlas::input ALIAS atlas-input)

target_include_directories(atlas-input PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-input PUBLIC atlas_project_options PRIVATE atlas_project_warnings)

# Backend selection (issue #68): which concrete atlas::input::RawSignalSource
# a build compiles in, resolved at configure time (spec §4: no runtime
# discovery) - never a runtime factory/plugin lookup. NullRawSignalSource
# (include/atlas/input/null_raw_signal_source.hpp) is header-only and always
# available regardless of this option; ATLAS_INPUT_BACKEND instead gates a
# *real* backend being compiled in on top of it. "SDL3" (this same issue) is
# the first real backend - keyboard/mouse/gamepad polling via SDL3's state-
# query API, reusing the exact SDL3 dependency atlas-render's
# ATLAS_RENDER_BACKEND=SDL3 already takes (FetchContent_Declare/
# _MakeAvailable are idempotent, so both libraries opting into SDL3 in the
# same build fetches it once). Selecting anything else fails the configure
# step with a clear message rather than silently building nothing.
set(ATLAS_INPUT_BACKEND
    "NULL"
    CACHE STRING "atlas-input backend to build: NULL (always available, no OS input) \
or SDL3 (issue #68 - keyboard/mouse/gamepad polling via SDL3)")
set_property(CACHE ATLAS_INPUT_BACKEND PROPERTY STRINGS "NULL" "SDL3")

if(ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  include(FetchContent)

  # Same cache-forced options and SYSTEM rationale as
  # libraries/atlas-render/CMakeLists.txt's own SDL3 FetchContent_Declare -
  # see that file's comments for why each of these matters, not just
  # tidiness. Declaring the exact same dependency (name, GIT_TAG) a second
  # time here is intentional and safe: FetchContent_Declare/_MakeAvailable
  # are idempotent, so a build with both ATLAS_RENDER_BACKEND=SDL3 and
  # ATLAS_INPUT_BACKEND=SDL3 fetches SDL3 exactly once and both libraries
  # share the same SDL3 target.
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

  target_sources(atlas-input PRIVATE src/sdl3_raw_signal_source.cpp)
  target_link_libraries(atlas-input PUBLIC SDL3::SDL3-static atlas::windowing)
elseif(NOT ATLAS_INPUT_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_INPUT_BACKEND='${ATLAS_INPUT_BACKEND}' is not a supported value - only NULL and SDL3 "
      "exist today")
endif()
