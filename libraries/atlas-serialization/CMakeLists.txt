add_library(atlas-serialization src/byte_writer.cpp src/byte_reader.cpp)
add_library(atlas::serialization ALIAS atlas-serialization)

target_include_directories(atlas-serialization PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                        $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-serialization PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
