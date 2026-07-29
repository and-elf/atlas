add_executable(atlas-stage-tests stage_sequence_test.cpp)

target_link_libraries(atlas-stage-tests PRIVATE atlas::stage GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-stage-tests)
