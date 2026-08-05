set(ATLAS_RENDER_TEST_SOURCES transform_test.cpp camera_test.cpp frame_builder_test.cpp
                               frame_backend_test.cpp mesh_asset_test.cpp texture_asset_test.cpp
                               skeleton_asset_test.cpp)

# Sdl3FrameBackend (issue #151) and its two resource-upload caches (issue
# #154) only exist - and only need testing - when atlas-render was itself
# configured to build them (ATLAS_RENDER_BACKEND=SDL3,
# libraries/atlas-render/CMakeLists.txt); the default NULL build never sees
# these files at all.
if(ATLAS_RENDER_BACKEND STREQUAL "SDL3")
  list(APPEND ATLAS_RENDER_TEST_SOURCES sdl3_frame_backend_test.cpp mesh_upload_cache_test.cpp
                                        texture_upload_cache_test.cpp sdl3_pixel_correctness_test.cpp
                                        sdl3_spinning_box_test.cpp sdl3_distance_cull_test.cpp)
endif()

add_executable(atlas-render-tests ${ATLAS_RENDER_TEST_SOURCES})

target_link_libraries(atlas-render-tests PRIVATE atlas::render GTest::gtest_main atlas_project_warnings)

# Real fixture asset files on disk (tests/atlas-render/fixtures/), not a
# mocked filesystem - matching CLAUDE.md's "test via a minimal test host, not
# by mocking behavior" rule and tests/atlas-resource/CMakeLists.txt's own
# precedent.
target_compile_definitions(
  atlas-render-tests PRIVATE ATLAS_RENDER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

if(ATLAS_RENDER_BACKEND STREQUAL "SDL3")
  # Issue #155: every test in this binary that actually reaches real shader
  # compilation (Sdl3MeshPipeline, via Sdl3FrameBackend or directly) hits a
  # genuine leak inside the vendored, prebuilt DirectXShaderCompiler shared
  # library - see lsan_suppressions.txt's own doc comment for the full
  # writeup and why this is a suppression, not a workaround for anything
  # this project's own code did wrong.
  set(ATLAS_RENDER_TEST_ENVIRONMENT
      "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan_suppressions.txt")

  # Also issue #155: the system Vulkan loader (not this project's code, nor
  # SDL/SDL_shadercross) enumerates *every* ICD registered under
  # /usr/share/vulkan/icd.d/ - this sandbox has several besides lavapipe
  # (gfxstream, nouveau, ...) for hardware that plainly is not present here.
  # Probing a rejected candidate driver during that enumeration was directly
  # observed (via a real, reproduced LeakSanitizer report, process-memory-map
  # dump included) to leak a small, fixed amount of memory on a background
  # thread the loader itself spawns - unresolvable to a specific function
  # for a suppressions-file entry above, since the offending ICD's shared
  # object is already dlclose()'d (and so unmapped/unsymbolizable) by the
  # time LeakSanitizer's exit-time check runs. Pointing VK_ICD_FILENAMES
  # straight at the one real, working ICD this sandbox has (Mesa's lavapipe,
  # apt package mesa-vulkan-drivers) sidesteps the whole class of problem by
  # construction: the loader then never dlopens any other candidate at all.
  # This is the standard Debian/Ubuntu install path (CLAUDE.md: Debian 13 is
  # this project's primary target) - guarded by EXISTS so a machine without
  # it at this exact path (a genuinely different distro, or lavapipe simply
  # absent) falls back to the loader's own default enumeration untouched,
  # preserving every fixture's existing GTEST_SKIP() path for "no GPU here
  # at all" rather than failing outright.
  if(EXISTS "/usr/share/vulkan/icd.d/lvp_icd.json")
    list(APPEND ATLAS_RENDER_TEST_ENVIRONMENT "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json")
  endif()

  # Applied to every test this binary discovers (not just the newly-passing
  # ones) since any of them may construct a real Sdl3MeshPipeline/GPU device.
  gtest_discover_tests(atlas-render-tests PROPERTIES ENVIRONMENT "${ATLAS_RENDER_TEST_ENVIRONMENT}")
else()
  gtest_discover_tests(atlas-render-tests)
endif()
