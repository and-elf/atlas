add_executable(atlas-resource-tests resource_id_test.cpp)

target_link_libraries(atlas-resource-tests PRIVATE atlas::resource GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-resource-tests)
