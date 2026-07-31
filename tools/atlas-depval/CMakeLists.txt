# atlas-depval links directly against atlas-cgen's own linkable library
# target (atlas::cgen_lib, tools/atlas-cgen/CMakeLists.txt) rather than
# redeclaring yaml-cpp or duplicating any manifest/graph/cycle-detection
# source - see tools/atlas-depval/README.md's "What this reuses vs. what
# this adds" section. atlas::cgen_lib already links yaml-cpp::yaml-cpp
# PUBLIC, so its include directories and target reach this tool
# transitively; nothing yaml-cpp-specific is declared here. This does
# introduce one real build-ordering requirement unlike atlas-rcc's
# sibling-tool independence: tools/CMakeLists.txt must add_subdirectory
# atlas-cgen before atlas-depval, since target_link_libraries below needs
# atlas-cgen-lib to already exist as a target.
add_library(atlas-depval-lib src/validator.cpp)
add_library(atlas::depval_lib ALIAS atlas-depval-lib)

target_include_directories(atlas-depval-lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                    $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-depval-lib PUBLIC atlas_project_options atlas::cgen_lib
                       PRIVATE atlas_project_warnings)

add_executable(atlas-depval src/main.cpp)
target_link_libraries(atlas-depval PRIVATE atlas::depval_lib atlas_project_warnings)
