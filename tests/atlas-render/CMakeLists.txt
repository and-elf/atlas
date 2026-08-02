add_executable(
  atlas-render-tests transform_test.cpp frame_builder_test.cpp frame_backend_test.cpp mesh_asset_test.cpp
                     texture_asset_test.cpp)

target_link_libraries(atlas-render-tests PRIVATE atlas::render GTest::gtest_main atlas_project_warnings)

# Real fixture asset files on disk (tests/atlas-render/fixtures/), not a
# mocked filesystem - matching CLAUDE.md's "test via a minimal test host, not
# by mocking behavior" rule and tests/atlas-resource/CMakeLists.txt's own
# precedent.
target_compile_definitions(
  atlas-render-tests PRIVATE ATLAS_RENDER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

gtest_discover_tests(atlas-render-tests)
