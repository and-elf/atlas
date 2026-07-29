add_library(atlas-stage src/stage_sequence.cpp)
add_library(atlas::stage ALIAS atlas-stage)

target_include_directories(atlas-stage PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-stage PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
