add_library(atlas-input src/scripted_raw_signal_source.cpp src/intent_router.cpp)
add_library(atlas::input ALIAS atlas-input)

target_include_directories(atlas-input PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-input PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
