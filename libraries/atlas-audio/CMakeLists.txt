add_library(atlas-audio src/sound_renderer.cpp src/wav_decoder.cpp src/decode_cache.cpp)
add_library(atlas::audio ALIAS atlas-audio)

target_include_directories(atlas-audio PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-audio PUBLIC atlas_project_options atlas::runtime atlas::resource
                                   PRIVATE atlas_project_warnings)

# Backend selection (issue #149): which concrete atlas::audio::AudioBackend a
# build compiles in, resolved at configure time (spec §4: no runtime
# discovery) - never a runtime factory/plugin lookup. NullAudioBackend
# (include/atlas/audio/null_audio_backend.hpp) is header-only and always
# available regardless of this option; ATLAS_AUDIO_BACKEND instead gates a
# *real* backend being compiled in on top of it. "SDL3" (issue #55) is the
# first real backend - one shared SDL3 playback device with a per-voice
# SDL_AudioStream, a hand-rolled equal-power pan law (SDL3 itself has no pan
# primitive), and a self-managed one-shot voice pool for trigger(), reusing
# the exact SDL3 dependency atlas-render's ATLAS_RENDER_BACKEND=SDL3/
# atlas-input's ATLAS_INPUT_BACKEND=SDL3 already take (FetchContent_Declare/
# _MakeAvailable are idempotent, so a build combining any of these SDL3
# options fetches SDL3 once). Selecting anything else fails the configure
# step with a clear message rather than silently building nothing.
set(ATLAS_AUDIO_BACKEND
    "NULL"
    CACHE STRING "atlas-audio backend to build: NULL (always available, no audio device) \
or SDL3 (issue #55 - SDL3 SDL_AudioStream-based device I/O)")
set_property(CACHE ATLAS_AUDIO_BACKEND PROPERTY STRINGS "NULL" "SDL3")

if(ATLAS_AUDIO_BACKEND STREQUAL "SDL3")
  include(FetchContent)

  # Same cache-forced options and SYSTEM rationale as
  # libraries/atlas-render/CMakeLists.txt's own SDL3 FetchContent_Declare -
  # see that file's comments for why each of these matters, not just
  # tidiness. Declaring the exact same dependency (name, GIT_TAG) again here
  # is intentional and safe: FetchContent_Declare/_MakeAvailable are
  # idempotent, so a build with ATLAS_RENDER_BACKEND=SDL3 and/or
  # ATLAS_INPUT_BACKEND=SDL3 alongside ATLAS_AUDIO_BACKEND=SDL3 fetches SDL3
  # exactly once and every library shares the same SDL3 target.
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

  # Third-party sources are out of scope for our clang-tidy gate
  # (CMAKE_CXX_CLANG_TIDY is a global default applied to every target,
  # including fetched dependencies).
  foreach(sdl3_target IN ITEMS SDL3-static SDL3 SDL_uclibc)
    if(TARGET ${sdl3_target})
      set_target_properties(${sdl3_target} PROPERTIES CXX_CLANG_TIDY "")
    endif()
  endforeach()

  target_sources(atlas-audio PRIVATE src/sdl3_audio_backend.cpp)
  target_link_libraries(atlas-audio PUBLIC SDL3::SDL3-static)
elseif(NOT ATLAS_AUDIO_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_AUDIO_BACKEND='${ATLAS_AUDIO_BACKEND}' is not a supported value - only NULL and SDL3 "
      "exist today")
endif()
