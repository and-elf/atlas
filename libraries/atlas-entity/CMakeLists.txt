add_library(atlas-entity src/entity_registry.cpp)
add_library(atlas::entity ALIAS atlas-entity)

target_include_directories(atlas-entity PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-entity PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
