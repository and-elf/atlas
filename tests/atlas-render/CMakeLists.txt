set(ATLAS_RENDER_TEST_SOURCES transform_test.cpp frame_builder_test.cpp frame_backend_test.cpp
                               mesh_asset_test.cpp texture_asset_test.cpp)

# Sdl3FrameBackend (issue #151) only exists - and only needs testing - when
# atlas-render was itself configured to build it (ATLAS_RENDER_BACKEND=SDL3,
# libraries/atlas-render/CMakeLists.txt); the default NULL build never sees
# this file at all.
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3")
  list(APPEND ATLAS_RENDER_TEST_SOURCES sdl3_frame_backend_test.cpp sdl3_shader_pipeline_test.cpp)
endif()

add_executable(atlas-render-tests ${ATLAS_RENDER_TEST_SOURCES})

target_link_libraries(atlas-render-tests PRIVATE atlas::render GTest::gtest_main atlas_project_warnings)

# Real fixture asset files on disk (tests/atlas-render/fixtures/), not a
# mocked filesystem - matching CLAUDE.md's "test via a minimal test host, not
# by mocking behavior" rule and tests/atlas-resource/CMakeLists.txt's own
# precedent.
target_compile_definitions(
  atlas-render-tests PRIVATE ATLAS_RENDER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

gtest_discover_tests(atlas-render-tests)
