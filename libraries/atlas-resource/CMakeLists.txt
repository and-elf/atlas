add_library(atlas-resource src/resource_id.cpp)
add_library(atlas::resource ALIAS atlas-resource)

target_include_directories(atlas-resource PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                  $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-resource PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
