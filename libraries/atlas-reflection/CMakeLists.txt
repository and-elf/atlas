add_library(atlas-reflection src/field_summary.cpp)
add_library(atlas::reflection ALIAS atlas-reflection)

target_include_directories(atlas-reflection PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                    $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-reflection PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
