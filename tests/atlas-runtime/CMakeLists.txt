add_executable(atlas-runtime-tests host_test.cpp)

target_link_libraries(atlas-runtime-tests PRIVATE atlas::runtime GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-runtime-tests)
