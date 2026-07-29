add_executable(atlas-contracts-tests contract_concepts_test.cpp contract_version_test.cpp)

target_link_libraries(atlas-contracts-tests PRIVATE atlas::contracts atlas::entity GTest::gtest_main
                                                     atlas_project_warnings)

gtest_discover_tests(atlas-contracts-tests)
