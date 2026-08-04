add_library(atlas-session src/in_memory_session_store.cpp)
add_library(atlas::session ALIAS atlas-session)

target_include_directories(atlas-session PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                  $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-session PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
