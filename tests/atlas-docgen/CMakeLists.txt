add_executable(atlas-docgen-tests markdown_writer_test.cpp)

target_link_libraries(atlas-docgen-tests PRIVATE atlas::docgen_lib GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-docgen-tests)
