add_library(
  atlas-render src/frame_builder.cpp src/mesh_asset.cpp src/texture_asset.cpp src/skeleton_asset.cpp
               src/animation_asset.cpp src/animation_decode_cache.cpp src/animation_sampler.cpp
               src/animation_pose_sync.cpp)
add_library(atlas::render ALIAS atlas-render)

target_include_directories(atlas-render PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-render
  PUBLIC atlas_project_options atlas::core atlas::entity atlas::resource atlas::runtime
  PRIVATE atlas_project_warnings)

# Backend selection (issue #148): which concrete atlas::render::FrameBackend
# a build compiles in, resolved at configure time (spec §4: no runtime
# discovery) - never a runtime factory/plugin lookup. NullFrameBackend
# (include/atlas/render/null_frame_backend.hpp) is header-only and always
# available regardless of this option; ATLAS_RENDER_BACKEND instead gates a
# *real* backend being compiled in on top of it. "SDL3" (issue #151, the
# first slice of #69) is the first real backend - window + SDL_GPU device
# bring-up, a clear-and-present loop, and (issue #154) real per-DrawCommand
# mesh/texture rendering. Selecting anything else fails the configure step
# with a clear message rather than silently building nothing.
set(ATLAS_RENDER_BACKEND
    "NULL"
    CACHE STRING "atlas-render backend to build: NULL (always available, no window/GPU) \
or SDL3 (issue #151 - SDL3/SDL_GPU window+device bring-up)")
set_property(CACHE ATLAS_RENDER_BACKEND PROPERTY STRINGS "NULL" "SDL3")

if(ATLAS_RENDER_BACKEND STREQUAL "SDL3")
  include(FetchContent)

  # Static, no shared lib to keep alongside test/tool binaries at run time -
  # matches this project's existing FetchContent precedent (yaml-cpp,
  # GoogleTest) of linking fetched dependencies statically. SDL_TEST_LIBRARY
  # defaults ON even when SDL3 is consumed as a subproject (SDL_TESTS/
  # SDL_EXAMPLES/SDL_INSTALL already default OFF in that case) - explicitly
  # turned off since this project has no use for SDL's own test harness.
  set(SDL_SHARED
      OFF
      CACHE BOOL "" FORCE)
  set(SDL_STATIC
      ON
      CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY
      OFF
      CACHE BOOL "" FORCE)

  # SYSTEM (CMake >= 3.25, our minimum) - see tools/atlas-cgen/CMakeLists.txt's
  # yaml-cpp FetchContent_Declare for why this matters, not just tidiness:
  # without it, a warning purely inside SDL3's own headers can still fail
  # our -Werror via whichever of our files includes them.
  FetchContent_Declare(
    SDL3 SYSTEM
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12)
  FetchContent_MakeAvailable(SDL3)

  # Third-party sources are out of scope for our clang-tidy gate
  # (CMAKE_CXX_CLANG_TIDY is a global default applied to every target,
  # including fetched dependencies).
  foreach(sdl3_target IN ITEMS SDL3-static SDL3 SDL_uclibc)
    if(TARGET ${sdl3_target})
      set_target_properties(${sdl3_target} PROPERTIES CXX_CLANG_TIDY "")
    endif()
  endforeach()

  # Shader/pipeline/draw-call path - DXC prebuilt binaries, SPIRV-Cross built
  # from source, and SDL_shadercross itself. Must run after SDL3's own
  # FetchContent_MakeAvailable above (see cmake/FetchShaderTooling.cmake's
  # own header comment for why).
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchShaderTooling.cmake")

  # The checked-in HLSL sources (shaders/*.hlsl) are embedded as constexpr
  # std::string_view headers at build time (the same technique
  # tools/atlas-cgen uses for its own templates, reimplemented independently
  # here - see cmake/EmbedShaderSource.cmake) so sdl3_mesh_pipeline.cpp never
  # needs to locate them on disk at runtime.
  set(ATLAS_RENDER_GENERATED_SHADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders")

  function(atlas_render_embed_shader shader_file cpp_name)
    add_custom_command(
      OUTPUT "${ATLAS_RENDER_GENERATED_SHADERS_DIR}/${cpp_name}.hpp"
      COMMAND
        ${CMAKE_COMMAND} -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_file}
        -DOUTPUT_FILE=${ATLAS_RENDER_GENERATED_SHADERS_DIR}/${cpp_name}.hpp -DCPP_NAME=${cpp_name}
        -DCPP_NAMESPACE=atlas::render::shaders -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderSource.cmake
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_file}"
              "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderSource.cmake"
      COMMENT "Embedding shader ${shader_file}"
      VERBATIM)
  endfunction()

  # issue #154: real mesh/texture shaders - supersede issue #153's
  # triangle.{vert,frag}.hlsl (removed as part of this issue).
  atlas_render_embed_shader(mesh.vert.hlsl mesh_vert_hlsl)
  atlas_render_embed_shader(mesh.frag.hlsl mesh_frag_hlsl)
  # issue #156: the GPU-driven distance-culling compute shader.
  atlas_render_embed_shader(distance_cull.comp.hlsl distance_cull_comp_hlsl)

  target_sources(
    atlas-render PRIVATE src/sdl3_frame_backend.cpp src/sdl3_mesh_pipeline.cpp
                         src/sdl3_distance_cull_pipeline.cpp src/mesh_upload_cache.cpp
                         src/texture_upload_cache.cpp
                         "${ATLAS_RENDER_GENERATED_SHADERS_DIR}/mesh_vert_hlsl.hpp"
                         "${ATLAS_RENDER_GENERATED_SHADERS_DIR}/mesh_frag_hlsl.hpp"
                         "${ATLAS_RENDER_GENERATED_SHADERS_DIR}/distance_cull_comp_hlsl.hpp")
  target_include_directories(atlas-render PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(atlas-render PUBLIC SDL3::SDL3-static
                                             SDL3_shadercross::SDL3_shadercross-static atlas::windowing)
elseif(NOT ATLAS_RENDER_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_RENDER_BACKEND='${ATLAS_RENDER_BACKEND}' is not a supported value - only NULL and "
      "SDL3 exist today")
endif()
