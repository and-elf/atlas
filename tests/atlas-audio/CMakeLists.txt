set(ATLAS_AUDIO_TEST_SOURCES sound_renderer_test.cpp audio_backend_test.cpp wav_decoder_test.cpp
                             decode_cache_test.cpp)

# Sdl3AudioBackend (issue #55) only exists - and only needs testing - when
# atlas-audio was itself configured to build it (ATLAS_AUDIO_BACKEND=SDL3,
# libraries/atlas-audio/CMakeLists.txt); the default NULL build never sees
# this file at all - mirrors tests/atlas-render/CMakeLists.txt's own
# ATLAS_RENDER_BACKEND=SDL3 conditional (and tests/atlas-input's
# ATLAS_INPUT_BACKEND=SDL3 one) exactly.
if(ATLAS_AUDIO_BACKEND STREQUAL "SDL3")
  list(APPEND ATLAS_AUDIO_TEST_SOURCES sdl3_audio_backend_test.cpp)
endif()

add_executable(atlas-audio-tests ${ATLAS_AUDIO_TEST_SOURCES})

target_link_libraries(atlas-audio-tests PRIVATE atlas::audio atlas::runtime atlas::resource GTest::gtest_main
                                                 atlas_project_warnings)

if(ATLAS_AUDIO_BACKEND STREQUAL "SDL3")
  # Forces SDL3's real, always-available "dummy" audio driver (verified
  # against the fetched SDL3 source: src/audio/dummy/SDL_dummyaudio.c) so
  # every test in this binary opens a real SDL3 audio device against a null
  # output sink rather than a real one - the same "real backend, no-op
  # hardware sink" headless-CI approach tests/atlas-input's own
  # Sdl3RawSignalSourceTest uses for SDL_HINT_VIDEO_DRIVER="offscreen".
  # SDL_AUDIODRIVER is the environment-variable form of SDL_HINT_AUDIO_DRIVER
  # (SDL_hints.h) - set as an environment variable, not a hint set from
  # inside the test binary itself, so it is already in effect before SDL's
  # own static/lazily-initialized driver list is built.
  gtest_discover_tests(atlas-audio-tests PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
else()
  gtest_discover_tests(atlas-audio-tests)
endif()
