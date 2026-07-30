add_library(atlas-render src/frame_builder.cpp)
add_library(atlas::render ALIAS atlas-render)

target_include_directories(atlas-render PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-render
  PUBLIC atlas_project_options atlas::core atlas::entity atlas::resource atlas::runtime
  PRIVATE atlas_project_warnings)
