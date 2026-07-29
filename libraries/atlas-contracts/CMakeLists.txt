add_library(atlas-contracts src/contract_version.cpp)
add_library(atlas::contracts ALIAS atlas-contracts)

target_include_directories(atlas-contracts PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                   $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-contracts PUBLIC atlas_project_options atlas::core PRIVATE atlas_project_warnings)
