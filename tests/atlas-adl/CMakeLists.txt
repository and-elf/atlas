add_executable(atlas-adl-tests asset_request_test.cpp)

target_link_libraries(atlas-adl-tests PRIVATE atlas::adl_lib GTest::gtest_main atlas_project_warnings)

# The real, non-fixture example request file lives under demo/design/requests/
# (issue #265's acceptance criteria: "at least one real example request file
# validates successfully") - not duplicated into tests/atlas-adl/fixtures/.
target_compile_definitions(
  atlas-adl-tests PRIVATE ATLAS_ADL_DEMO_REQUESTS_DIR="${CMAKE_SOURCE_DIR}/demo/design/requests")

gtest_discover_tests(atlas-adl-tests)
