add_executable(atlas-request-tests request_result_test.cpp request_diagnostics_test.cpp dispatch_test.cpp)

target_link_libraries(atlas-request-tests PRIVATE atlas::request atlas::entity GTest::gtest_main
                                                   atlas_project_warnings)

gtest_discover_tests(atlas-request-tests)
