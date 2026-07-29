add_executable(atlas-scheduler-tests scheduler_test.cpp)

target_link_libraries(atlas-scheduler-tests PRIVATE atlas::scheduler GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-scheduler-tests)
