add_library(atlas-core src/semantic_version.cpp)
add_library(atlas::core ALIAS atlas-core)

target_include_directories(atlas-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                              $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-core PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
