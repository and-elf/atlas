add_executable(
  atlas-diagnostics-tests severity_test.cpp record_test.cpp logger_test.cpp console_sink_test.cpp
                          integration_test.cpp)

target_link_libraries(atlas-diagnostics-tests PRIVATE atlas::diagnostics GTest::gtest_main
                                                       atlas_project_warnings)

gtest_discover_tests(atlas-diagnostics-tests)
