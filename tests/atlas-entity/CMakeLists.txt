add_executable(atlas-entity-tests entity_registry_test.cpp)

target_link_libraries(atlas-entity-tests PRIVATE atlas::entity GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-entity-tests)
