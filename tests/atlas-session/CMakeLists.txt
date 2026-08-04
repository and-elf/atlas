add_executable(atlas-session-tests session_id_test.cpp in_memory_session_store_test.cpp)

target_link_libraries(atlas-session-tests PRIVATE atlas::session GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-session-tests)
