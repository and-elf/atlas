add_executable(atlas-ui-tests transform_test.cpp compositing_test.cpp bindable_property_test.cpp
                               clickable_test.cpp node_test.cpp)

target_link_libraries(atlas-ui-tests PRIVATE atlas::ui GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-ui-tests)
