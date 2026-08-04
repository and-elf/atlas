set(ATLAS_REPLICATION_TEST_SOURCES
    entity_ref_codec_test.cpp resource_id_codec_test.cpp property_id_test.cpp property_id_codec_test.cpp
    property_codec_test.cpp null_transport_test.cpp)

# UnixSocketTransport (issue #218) only exists - and only needs testing - when
# atlas-replication was itself configured to build it
# (ATLAS_REPLICATION_TRANSPORT=UNIX, libraries/atlas-replication/CMakeLists.txt);
# the default NULL build never sees this file at all. Mirrors
# tests/atlas-physics/CMakeLists.txt's own ATLAS_PHYSICS_BACKEND-conditional
# sources pattern exactly.
if(ATLAS_REPLICATION_TRANSPORT STREQUAL "UNIX")
  list(APPEND ATLAS_REPLICATION_TEST_SOURCES unix_socket_transport_test.cpp)
endif()

add_executable(atlas-replication-tests ${ATLAS_REPLICATION_TEST_SOURCES})

target_link_libraries(atlas-replication-tests PRIVATE atlas::replication GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-replication-tests)
