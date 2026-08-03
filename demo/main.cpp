#include "app.hpp"

// demo-host (issue #70, App shape per PR #185 review feedback): the actual
// process entry point. Everything demo-host does - composing a real
// Host+Context+DemoRuntimeHost, running a real-time-paced tick loop, argv
// parsing, default SIGINT/SIGTERM handling - lives in atlas::demo::App
// (app.hpp/app.cpp), unit tested there (demo/tests/app_test.cpp). This file
// itself is not unit tested - constructing the real App and calling run()
// is exactly the "CLI entry point" convention tools/atlas-cgen/src/main.cpp
// documents and cmake/CodeCoverage.cmake excludes this file the same way.
int main(int argc, char** argv) {
    atlas::demo::App app(argc, argv);
    return app.run();
}
