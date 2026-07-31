add_executable(atlas-render-tests transform_test.cpp frame_builder_test.cpp)

target_link_libraries(atlas-render-tests PRIVATE atlas::render GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-render-tests)
