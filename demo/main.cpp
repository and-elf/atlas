#include "presentation_app.hpp"

#ifdef ATLAS_DEMO_REAL_SDL3_BACKEND
#    include "atlas/input/sdl3_raw_signal_source.hpp"
#    include "atlas/render/sdl3_frame_backend.hpp"
#    include "atlas/resource/resource_registry.hpp"
#    include "atlas/windowing/sdl3_shared_window.hpp"
#else
#    include "atlas/input/null_raw_signal_source.hpp"
#    include "atlas/render/null_frame_backend.hpp"
#endif

// demo-host (issue #70, App shape per PR #185 review feedback; issue #71
// parts 1/2 for the PresentationApp variant below): the actual process
// entry point. Everything demo-host does - composing a real Host+Context+
// DemoRuntimeHost, running a real-time-paced tick loop, argv parsing,
// default SIGINT/SIGTERM handling, and now polling input/building frames/
// rendering audio each tick - lives in atlas::demo::App/PresentationApp
// (app.hpp/app.cpp, presentation_app.hpp), unit tested there
// (demo/tests/app_test.cpp, demo/tests/presentation_app_test.cpp). This
// file itself is not unit tested - constructing the real App and calling
// run() is exactly the "CLI entry point" convention
// tools/atlas-cgen/src/main.cpp documents and cmake/CodeCoverage.cmake
// excludes this file the same way.
//
// ATLAS_DEMO_REAL_SDL3_BACKEND (issue #197): demo/CMakeLists.txt defines
// this only when both ATLAS_RENDER_BACKEND and ATLAS_INPUT_BACKEND are
// SDL3 - a configure-time choice, never a runtime factory/plugin lookup
// (spec §4), matching every other real-vs-null backend selection in this
// codebase. The real branch below constructs one Sdl3SharedWindow (#174) so
// Sdl3RawSignalSource and Sdl3FrameBackend present into/read focus from the
// same single window, plus an empty ResourceRegistry - no real mesh/
// material assets exist yet (#194's own scope note, unchanged by this
// issue), so build_frame legitimately draws nothing even with a real GPU
// backend running the swapchain clear/present loop. Every other CI leg
// (which stays on the default NULL/NULL backends) takes the unchanged Null
// branch.
int main(int argc, char** argv) {
#ifdef ATLAS_DEMO_REAL_SDL3_BACKEND
    atlas::windowing::Sdl3SharedWindow window;
    const atlas::resource::ResourceRegistry registry{{}};
    atlas::demo::PresentationApp<atlas::input::Sdl3RawSignalSource, atlas::render::Sdl3FrameBackend> app(
        argc,
        argv,
        atlas::input::Sdl3RawSignalSource{window},
        atlas::render::Sdl3FrameBackend{registry, window});
#else
    atlas::demo::PresentationApp<atlas::input::NullRawSignalSource, atlas::render::NullFrameBackend> app(
        argc, argv, atlas::input::NullRawSignalSource{}, atlas::render::NullFrameBackend{});
#endif
    return app.run();
}
