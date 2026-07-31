add_library(atlas-audio src/sound_renderer.cpp)
add_library(atlas::audio ALIAS atlas-audio)

target_include_directories(atlas-audio PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-audio PUBLIC atlas_project_options atlas::runtime atlas::resource
                                   PRIVATE atlas_project_warnings)
