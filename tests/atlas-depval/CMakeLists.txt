add_executable(atlas-depval-tests validator_test.cpp)

target_link_libraries(atlas-depval-tests PRIVATE atlas::depval_lib atlas::cgen_lib GTest::gtest_main
                                                  atlas_project_warnings)

gtest_discover_tests(atlas-depval-tests)
