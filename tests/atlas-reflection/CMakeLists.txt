add_executable(atlas-reflection-tests field_count_test.cpp field_summary_test.cpp)

target_link_libraries(atlas-reflection-tests PRIVATE atlas::reflection atlas::entity GTest::gtest_main
                                                      atlas_project_warnings)

gtest_discover_tests(atlas-reflection-tests)
