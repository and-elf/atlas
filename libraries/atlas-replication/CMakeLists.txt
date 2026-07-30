add_library(atlas-replication src/entity_ref_codec.cpp src/resource_id_codec.cpp src/property_id.cpp
                              src/property_id_codec.cpp)
add_library(atlas::replication ALIAS atlas-replication)

target_include_directories(atlas-replication PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                      $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-replication
  PUBLIC atlas_project_options atlas::entity atlas::resource atlas::serialization atlas::reflection
  PRIVATE atlas_project_warnings)
