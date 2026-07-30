add_executable(demo-tests combat_scenario_test.cpp equipment_test.cpp health_test.cpp armor_test.cpp
                          movement_test.cpp healing_test.cpp pathing_test.cpp aura_test.cpp
                          line_of_sight_test.cpp)

target_link_libraries(demo-tests PRIVATE demo-capabilities GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(demo-tests)
