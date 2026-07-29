add_executable(atlas-serialization-tests byte_stream_test.cpp byte_stream_float_test.cpp)

target_link_libraries(atlas-serialization-tests PRIVATE atlas::serialization GTest::gtest_main
                                                         atlas_project_warnings)

gtest_discover_tests(atlas-serialization-tests)
