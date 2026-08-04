add_executable(
  atlas-replication-tests entity_ref_codec_test.cpp resource_id_codec_test.cpp property_id_test.cpp
                          property_id_codec_test.cpp property_codec_test.cpp null_transport_test.cpp)

target_link_libraries(atlas-replication-tests PRIVATE atlas::replication GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-replication-tests)
