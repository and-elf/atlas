add_library(atlas-request src/request_diagnostics.cpp)
add_library(atlas::request ALIAS atlas-request)

target_include_directories(atlas-request PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                 $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-request
  PUBLIC atlas_project_options atlas::contracts atlas::runtime
  PRIVATE atlas_project_warnings)
