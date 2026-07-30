# Header-only (issue #23): ResourceId::from_name is now constexpr, which
# means its definition must live in the header, not a separately-compiled
# .cpp - resource_id.hpp was this library's only source file, so there is
# nothing left to compile. INTERFACE is the correct CMake shape for that,
# not a workaround: there's no object file, no atlas_project_warnings to
# apply to a compilation that no longer happens - warnings on code that
# includes this header come from whatever target compiles that code, which
# already links its own atlas_project_warnings.
add_library(atlas-resource INTERFACE)
add_library(atlas::resource ALIAS atlas-resource)

target_include_directories(atlas-resource INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                     $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-resource INTERFACE atlas_project_options)
