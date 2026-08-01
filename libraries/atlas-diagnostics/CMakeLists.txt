add_library(atlas-diagnostics src/severity.cpp src/console_sink.cpp)
add_library(atlas::diagnostics ALIAS atlas-diagnostics)

target_include_directories(atlas-diagnostics PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                     $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-diagnostics PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
