add_library(atlas-scheduler src/scheduler.cpp)
add_library(atlas::scheduler ALIAS atlas-scheduler)

target_include_directories(atlas-scheduler PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                   $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-scheduler PUBLIC atlas_project_options atlas::stage PRIVATE atlas_project_warnings)
