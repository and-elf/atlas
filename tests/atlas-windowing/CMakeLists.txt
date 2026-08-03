# Sdl3SharedWindow (issue #174) only exists - and only needs testing - when
# atlas-windowing was itself configured to build it (ATLAS_RENDER_BACKEND=SDL3
# or ATLAS_INPUT_BACKEND=SDL3, libraries/atlas-windowing/CMakeLists.txt); the
# default (neither backend selecting SDL3) build never sees this file at all,
# mirroring tests/atlas-render/CMakeLists.txt's own ATLAS_RENDER_BACKEND gate.
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3" OR ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  add_executable(atlas-windowing-tests sdl3_shared_window_test.cpp)
  target_link_libraries(atlas-windowing-tests PRIVATE atlas::windowing GTest::gtest_main
                                                       atlas_project_warnings)
  gtest_discover_tests(atlas-windowing-tests)
endif()
