add_executable(atlas-audio-tests sound_renderer_test.cpp audio_backend_test.cpp)

target_link_libraries(atlas-audio-tests PRIVATE atlas::audio atlas::runtime GTest::gtest_main
                                                 atlas_project_warnings)

gtest_discover_tests(atlas-audio-tests)
