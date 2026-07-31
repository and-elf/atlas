add_executable(atlas-input-tests intent_test.cpp raw_signal_test.cpp binding_test.cpp intent_router_test.cpp)

target_link_libraries(atlas-input-tests PRIVATE atlas::input GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-input-tests)
