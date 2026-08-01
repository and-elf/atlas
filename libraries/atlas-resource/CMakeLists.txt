# No longer header-only (issue #66 added ResourceRegistry, which needs real
# filesystem I/O in a .cpp) - resource_id.hpp's own from_name is still
# constexpr and stays header-only itself, but the library as a whole now has
# an object file to compile, so it is a regular (non-INTERFACE) target again.
add_library(atlas-resource src/resource_registry.cpp)
add_library(atlas::resource ALIAS atlas-resource)

target_include_directories(atlas-resource PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                  $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-resource PUBLIC atlas_project_options PRIVATE atlas_project_warnings)
