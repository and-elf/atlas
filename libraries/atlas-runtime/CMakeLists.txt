add_library(atlas-runtime src/host.cpp)
add_library(atlas::runtime ALIAS atlas-runtime)

target_include_directories(atlas-runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                 $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-runtime
  PUBLIC atlas_project_options atlas::entity atlas::scheduler atlas::stage
  PRIVATE atlas_project_warnings)
