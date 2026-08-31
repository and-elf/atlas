# Issue #153: fetches/builds the shader cross-compilation toolchain needed to
# turn this library's HLSL source (libraries/atlas-render/shaders/) into
# SPIR-V (and, for free, DXIL/MSL - the same DXC binary emits all three) at
# build time. Included exactly once from CMakeLists.txt's
# `ATLAS_RENDER_BACKEND STREQUAL "SDL3"` block, after SDL3's own
# FetchContent_MakeAvailable(SDL3) - so SDL_shadercross's own guarded
# find_package(SDL3) call (it checks for SDL3::Headers/SDL3::SDL3/
# SDL3::SDL3-static-or-shared first) finds our already-fetched SDL3 targets
# and skips find_package(SDL3) entirely. Verified empirically for this round
# (no "Found SDL3" message from SDL_shadercross's own configure output) -
# not assumed from a prior read of its CMakeLists.txt alone.
#
# Three pieces, each verified against the real upstream source at the pinned
# commit (not a summary of it - see this library's README, "Scoping
# decisions", for the two-round history that arrived at this design):
#
# 1. Microsoft's official prebuilt DirectXShaderCompiler (DXC) binaries -
#    DXC is SDL_shadercross's only HLSL front-end (SDL_ShaderCross_CompileSPIRVFromHLSL()
#    unconditionally routes through it), fetched the exact way
#    SDL_shadercross's own CI does
#    (build-scripts/download-prebuilt-DirectXShaderCompiler.cmake at the
#    pinned commit): release v1.9.2602, linux_dxc_2026_02_20.x86_64.tar.gz,
#    extracted under <root>/linux so FindDirectXShaderCompiler.cmake's
#    "linux/include/dxc" / "linux/lib" path suffixes resolve. The URL/hash
#    below were copied from that real script file, then independently
#    verified with a real download + sha256sum in this round (see README) -
#    not blind-copied.
#
# 2. SPIRV-Cross, built from source at the exact commit SDL_shadercross's own
#    .gitmodules submodule pointer resolves to at the pinned commit (`git
#    ls-tree <pinned-commit> external/SPIRV-Cross`, not the floating `main`
#    branch .gitmodules itself names) - NOT the apt-installable
#    libspirv-cross-c-shared-dev package. That package's 2021-vintage API
#    predates symbols this pinned SDL_shadercross commit actually calls
#    (spvc_msl_resource_binding_2, SPVC_COMPILER_OPTION_HLSL_USE_ENTRY_POINT_NAME) -
#    verified empirically by actually trying it first (roughly 90 real
#    compiler errors), not assumed from the issue's own plan text, which
#    said only "confirmed installable" and never checked its API surface.
#    Built as a standalone configure+build+install (an execute_process
#    "sub-build"), matching exactly how SDL_shadercross's own CI builds
#    SPIRV-Cross on its Ubuntu 24.04 leg (a separate step entirely outside
#    SDL_shadercross's own SDLSHADERCROSS_VENDORED machinery, which builds
#    SPIRV-Cross+DXC together from git submodules INSIDE SDL_shadercross's
#    own subdirectory - not what this does here) - add_subdirectory()'ing
#    SPIRV-Cross into our own build would only produce bare, non-exported
#    targets; SDL_shadercross's own non-vendored path calls
#    find_package(spirv_cross_c_shared REQUIRED), which needs a real
#    installed <name>Config.cmake package, only a genuine `cmake --install`
#    produces.
#
# 3. SDL_shadercross itself, SDLSHADERCROSS_VENDORED=OFF so it consumes (1)
#    and (2) above via find_package() rather than building either from a git
#    submodule of its own.
#
# Issue #273: on Linux, the Linux/SPIRV-Cross/Vulkan path above is exercised
# end-to-end by this project's own sandbox/CI. macOS and Windows instead use
# SDL_shadercross's own SDLSHADERCROSS_VENDORED=ON mode (its CMakeLists.txt
# add_subdirectory()'s external/SPIRV-Cross, external/SPIRV-Headers,
# external/SPIRV-Tools and external/DirectXShaderCompiler - all pulled in as
# git submodules of the pinned SDL_shadercross commit - and builds everything
# from source), the same mechanism #271's reference script
# (https://gist.githubusercontent.com/trojanfoe/aae4fe796c7bb8a58fd53d6562b4400d)
# uses for all three platforms. This sidesteps needing a per-platform
# prebuilt DXC binary at all - the reason the Linux-only piece 1/2 approach
# above can't extend to macOS (Microsoft publishes no macOS DXC binary,
# prebuilt or otherwise). Still unverified end-to-end on real macOS/Windows
# hardware by this project's own CI - see README's Open Questions.

include(FetchContent)

set(SDLSHADERCROSS_SHARED
    OFF
    CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_STATIC
    ON
    CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI
    OFF
    CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_INSTALL
    OFF
    CACHE BOOL "" FORCE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # --- 1. Prebuilt DXC binaries -----------------------------------------------
  set(ATLAS_RENDER_DXC_ROOT "${CMAKE_CURRENT_BINARY_DIR}/_deps/directxshadercompiler-binaries")

  FetchContent_Declare(
    atlas_render_dxc_linux
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz
    URL_HASH SHA256=a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60
    SOURCE_DIR "${ATLAS_RENDER_DXC_ROOT}/linux")
  FetchContent_MakeAvailable(atlas_render_dxc_linux)

  # --- 2. SPIRV-Cross, built from source at SDL_shadercross's own pinned commit
  set(ATLAS_RENDER_SPIRV_CROSS_COMMIT 1a6169566c73d3da552748fc372fe2bbb856e46e)
  set(ATLAS_RENDER_SPIRV_CROSS_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/spirv-cross-install")

  FetchContent_Declare(
    atlas_render_spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG ${ATLAS_RENDER_SPIRV_CROSS_COMMIT})
  FetchContent_GetProperties(atlas_render_spirv_cross)
  if(NOT atlas_render_spirv_cross_POPULATED)
    # Deliberately FetchContent_Populate(), not FetchContent_MakeAvailable() -
    # SPIRV-Cross does have its own top-level CMakeLists.txt, so MakeAvailable
    # would add_subdirectory() it into our own build graph, which is exactly
    # what this needs to avoid (see the file header comment above for why a
    # real `cmake --install` is needed instead).
    FetchContent_Populate(atlas_render_spirv_cross)
  endif()

  # A stamp file guards the actual configure+build+install so an ordinary
  # reconfigure of THIS project (no change to the pinned SPIRV-Cross commit)
  # doesn't recompile SPIRV-Cross every time - only a fresh build tree, or
  # bumping ATLAS_RENDER_SPIRV_CROSS_COMMIT above, does.
  set(atlas_render_spirv_cross_stamp
      "${ATLAS_RENDER_SPIRV_CROSS_INSTALL_PREFIX}/.atlas-render-built-${ATLAS_RENDER_SPIRV_CROSS_COMMIT}")

  if(NOT EXISTS "${atlas_render_spirv_cross_stamp}")
    message(STATUS "atlas-render: configuring/building/installing SPIRV-Cross "
                   "${ATLAS_RENDER_SPIRV_CROSS_COMMIT} (one-time per build tree - the apt "
                   "package's API predates what this pinned SDL_shadercross commit needs, see "
                   "libraries/atlas-render/README.md)")

    execute_process(
      COMMAND
        "${CMAKE_COMMAND}" -S "${atlas_render_spirv_cross_SOURCE_DIR}" -B
        "${atlas_render_spirv_cross_BINARY_DIR}" -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        -DSPIRV_CROSS_SHARED=ON -DSPIRV_CROSS_STATIC=ON -DSPIRV_CROSS_CLI=OFF
        -DSPIRV_CROSS_ENABLE_TESTS=OFF "-DCMAKE_INSTALL_PREFIX=${ATLAS_RENDER_SPIRV_CROSS_INSTALL_PREFIX}"
      RESULT_VARIABLE atlas_render_spirv_cross_configure_result)
    if(NOT atlas_render_spirv_cross_configure_result EQUAL 0)
      message(FATAL_ERROR "atlas-render: SPIRV-Cross configure failed (exit code "
                           "${atlas_render_spirv_cross_configure_result})")
    endif()

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${atlas_render_spirv_cross_BINARY_DIR}" --parallel
      RESULT_VARIABLE atlas_render_spirv_cross_build_result)
    if(NOT atlas_render_spirv_cross_build_result EQUAL 0)
      message(FATAL_ERROR "atlas-render: SPIRV-Cross build failed (exit code "
                           "${atlas_render_spirv_cross_build_result})")
    endif()

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --install "${atlas_render_spirv_cross_BINARY_DIR}"
      RESULT_VARIABLE atlas_render_spirv_cross_install_result)
    if(NOT atlas_render_spirv_cross_install_result EQUAL 0)
      message(FATAL_ERROR "atlas-render: SPIRV-Cross install failed (exit code "
                           "${atlas_render_spirv_cross_install_result})")
    endif()

    file(WRITE "${atlas_render_spirv_cross_stamp}" "")
  endif()

  # --- 3. SDL_shadercross itself, SDLSHADERCROSS_VENDORED=OFF -----------------
  # Environment variables, not -D cache variables - verified empirically, not
  # assumed: SDL_shadercross's own CMakeLists.txt unconditionally does
  # `set(DirectXShaderCompiler_ROOT ...)` (a plain, non-CACHE variable) right
  # before its own find_package(DirectXShaderCompiler REQUIRED) call, which
  # would shadow any -D cache value in that same directory scope. CMake's
  # <PackageName>_ROOT policy (CMP0074) makes find_package()'s own nested
  # find_path/find_library/Config-mode search also consult
  # $ENV{<PackageName>_ROOT} regardless of that local shadowing - which is
  # exactly how SDL_shadercross's own CI sets these two variables too (GITHUB_ENV,
  # i.e. the process environment of the subsequent configure step), not a
  # command-line flag, despite what a first read of the issue's own plan text
  # assumed.
  set(ENV{DirectXShaderCompiler_ROOT} "${ATLAS_RENDER_DXC_ROOT}")
  set(ENV{spirv_cross_c_shared_ROOT} "${ATLAS_RENDER_SPIRV_CROSS_INSTALL_PREFIX}")

  set(SDLSHADERCROSS_VENDORED
      OFF
      CACHE BOOL "" FORCE)

  # GIT_SUBMODULES "" - SDL_shadercross's own submodules (SPIRV-Cross,
  # SPIRV-Headers, SPIRV-Tools, DirectXShaderCompiler, its own vendored-from-source
  # fallbacks) are irrelevant here (SDLSHADERCROSS_VENDORED=OFF above) and
  # would otherwise be recursively cloned in full (DirectXShaderCompiler's own
  # history alone is gigabytes) for no benefit.
  FetchContent_Declare(
    SDL_shadercross SYSTEM
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
    GIT_TAG e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba
    GIT_SUBMODULES "")
else()
  # --- macOS/Windows: SDLSHADERCROSS_VENDORED=ON, DXC+SPIRV-Cross built from
  # source by SDL_shadercross's own CMakeLists.txt via its git submodules ----
  set(SDLSHADERCROSS_DXC
      ON
      CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_VENDORED
      ON
      CACHE BOOL "" FORCE)

  # No GIT_SUBMODULES restriction here (unlike the Linux branch above) -
  # vendored mode needs external/SPIRV-Cross, external/SPIRV-Headers,
  # external/SPIRV-Tools and external/DirectXShaderCompiler all populated,
  # so the default (recursive clone of every submodule) is what's wanted.
  FetchContent_Declare(
    SDL_shadercross SYSTEM
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
    GIT_TAG e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba)
endif()

# Third-party sources are out of scope for our clang-tidy gate
# (CMAKE_CXX_CLANG_TIDY is a global default applied to every target,
# including fetched dependencies) - same precedent as SDL3-static/yaml-cpp.
# Saving/restoring the variable around MakeAvailable(), rather than setting
# CXX_CLANG_TIDY on specific target names afterward, is what's needed for the
# macOS/Windows vendored branch above: it add_subdirectory()'s several more
# third-party projects (SPIRV-Cross, SPIRV-Headers, SPIRV-Tools,
# DirectXShaderCompiler) whose target names this file doesn't enumerate.
set(atlas_render_saved_clang_tidy "${CMAKE_CXX_CLANG_TIDY}")
set(CMAKE_CXX_CLANG_TIDY "")
FetchContent_MakeAvailable(SDL_shadercross)
set(CMAKE_CXX_CLANG_TIDY "${atlas_render_saved_clang_tidy}")
