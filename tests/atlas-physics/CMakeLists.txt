add_executable(atlas-physics-tests null_physics_backend_test.cpp)

target_link_libraries(atlas-physics-tests PRIVATE atlas::physics GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-physics-tests)
