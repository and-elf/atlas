add_executable(
  atlas-rcc-tests resource_manifest_test.cpp resource_table_test.cpp resource_blob_test.cpp
                 host_resource_manifest_test.cpp host_resource_compilation_test.cpp)

target_link_libraries(atlas-rcc-tests PRIVATE atlas::rcc_lib atlas::resource GTest::gtest_main
                                               atlas_project_warnings)

# Real fixture asset files on disk (tests/atlas-rcc/fixtures/), not a mocked
# filesystem - matching CLAUDE.md's "test via a minimal test host, not by
# mocking behavior" rule.
target_compile_definitions(atlas-rcc-tests PRIVATE ATLAS_RCC_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

gtest_discover_tests(atlas-rcc-tests)
