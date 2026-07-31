add_executable(atlas-rcc-tests resource_manifest_test.cpp resource_table_test.cpp)

target_link_libraries(atlas-rcc-tests PRIVATE atlas::rcc_lib atlas::resource GTest::gtest_main
                                               atlas_project_warnings)

gtest_discover_tests(atlas-rcc-tests)
