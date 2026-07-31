add_executable(atlas-resource-tests resource_id_test.cpp resource_registry_test.cpp)

target_link_libraries(atlas-resource-tests PRIVATE atlas::resource GTest::gtest_main atlas_project_warnings)

# Real fixture asset files on disk (tests/atlas-resource/fixtures/), not a
# mocked filesystem - matching CLAUDE.md's "test via a minimal test host, not
# by mocking behavior" rule.
target_compile_definitions(
  atlas-resource-tests
  PRIVATE ATLAS_RESOURCE_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

gtest_discover_tests(atlas-resource-tests)
