add_executable(atlas-core-tests semantic_version_test.cpp)

target_link_libraries(atlas-core-tests PRIVATE atlas::core GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-core-tests)
