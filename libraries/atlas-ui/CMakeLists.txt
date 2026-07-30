add_library(atlas-ui src/clickable.cpp src/node.cpp)
add_library(atlas::ui ALIAS atlas-ui)

target_include_directories(atlas-ui PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                            $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-ui
  PUBLIC atlas_project_options atlas::entity atlas::resource atlas::runtime
  PRIVATE atlas_project_warnings)
