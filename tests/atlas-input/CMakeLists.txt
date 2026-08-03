set(ATLAS_INPUT_TEST_SOURCES intent_test.cpp raw_signal_test.cpp binding_test.cpp intent_router_test.cpp
                             null_raw_signal_source_test.cpp)

# Sdl3RawSignalSource (issue #68) only exists - and only needs testing - when
# atlas-input was itself configured to build it (ATLAS_INPUT_BACKEND=SDL3,
# libraries/atlas-input/CMakeLists.txt); the default NULL build never sees
# this file at all - mirrors tests/atlas-render/CMakeLists.txt's own
# ATLAS_RENDER_BACKEND=SDL3 conditional exactly.
if(ATLAS_INPUT_BACKEND STREQUAL "SDL3")
  list(APPEND ATLAS_INPUT_TEST_SOURCES sdl3_raw_signal_source_test.cpp)
endif()

add_executable(atlas-input-tests ${ATLAS_INPUT_TEST_SOURCES})

target_link_libraries(atlas-input-tests PRIVATE atlas::input GTest::gtest_main atlas_project_warnings)

gtest_discover_tests(atlas-input-tests)
