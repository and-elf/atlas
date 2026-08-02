add_library(atlas-audio src/sound_renderer.cpp src/wav_decoder.cpp)
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
# *real* backend (issue #55, not yet implemented) being compiled in. Only
# "NULL" exists today - selecting anything else fails the configure step
# with a clear message rather than silently building nothing.
set(ATLAS_AUDIO_BACKEND
    "NULL"
    CACHE STRING "atlas-audio backend to build: NULL (always available, no audio device) \
or a real backend once issue #55 lands")
set_property(CACHE ATLAS_AUDIO_BACKEND PROPERTY STRINGS "NULL")

if(NOT ATLAS_AUDIO_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_AUDIO_BACKEND='${ATLAS_AUDIO_BACKEND}' is not implemented yet - only NULL exists "
      "today; see issue #55 for the real backend this option will eventually gate")
endif()
