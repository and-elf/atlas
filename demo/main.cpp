#include "atlas/input/null_raw_signal_source.hpp"

#include "presentation_app.hpp"

// demo-host (issue #70, App shape per PR #185 review feedback; issue #71
// part 1 for the PresentationApp variant below): the actual process entry
// point. Everything demo-host does - composing a real Host+Context+
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
// NullRawSignalSource, not Sdl3RawSignalSource: this issue lands the
// input/render/audio mechanism against the Null/scripted backends only (see
// issue #194's scope note) - the real SDL3 backends, via #174's shared
// window, are a separate follow-up issue.
int main(int argc, char** argv) {
    atlas::demo::PresentationApp<atlas::input::NullRawSignalSource> app(
        argc, argv, atlas::input::NullRawSignalSource{});
    return app.run();
}
