#include "atlas/input/sdl3_raw_signal_source.hpp"
#include "atlas/render/sdl3_frame_backend.hpp"
#include "atlas/resource/resource_registry.hpp"
#include "atlas/windowing/sdl3_shared_window.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "presentation_app.hpp"

namespace atlas::demo {
namespace {

// A real argv-shaped array (argv[argc] must be a null pointer, the same
// contract main()'s own argv has) - mirrors app_test.cpp's own Argv, kept as
// a separate small copy here rather than shared test scaffolding for one
// call site.
class Argv {
public:
    explicit Argv(std::initializer_list<const char*> args) : args_(args.begin(), args.end()) {
        pointers_.reserve(args_.size() + 1);
        for (auto& arg : args_) {
            pointers_.push_back(arg.data());
        }
        pointers_.push_back(nullptr);
    }

    [[nodiscard]] int argc() const { return static_cast<int>(args_.size()); }
    [[nodiscard]] char** argv() { return pointers_.data(); }

private:
    std::vector<std::string> args_;
    std::vector<char*> pointers_;
};

// issue #197 (#71 part 2): proves PresentationApp actually composes with the
// real Sdl3RawSignalSource/Sdl3FrameBackend sharing one Sdl3SharedWindow
// (#174) end to end - main.cpp's own ATLAS_DEMO_REAL_SDL3_BACKEND branch
// already proves this compiles/links at build time (this CMake target only
// exists in that same configuration), this proves it actually runs.
//
// A machine with no real/software GPU or Vulkan/Metal/D3D12 ICD at all is
// exactly why NullFrameBackend exists in the first place - mirrors
// tests/atlas-render/sdl3_frame_backend_test.cpp's own SetUp()/GTEST_SKIP()
// precedent exactly, including forcing SDL_HINT_VIDEO_DRIVER to "offscreen"
// (not "dummy" - see that file's own comment for why "dummy" would
// structurally guarantee a skip even with a working software GPU present).
class PresentationAppSdl3Test : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

        try {
            window_.emplace("atlas-demo-tests", 64, 64, SDL_WINDOW_HIDDEN);
            // No real mesh/material assets exist yet (#194's own scope note,
            // unchanged by this issue) - an empty registry is exactly
            // consistent with that: build_frame legitimately draws nothing
            // even against a real GPU device running the swapchain
            // clear/present loop.
            registry_.emplace(std::unordered_map<std::string, std::filesystem::path>{});
            frame_backend_.emplace(*registry_, *window_);
            source_.emplace(*window_);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << "No real SDL3/SDL_GPU-capable backend available in this environment "
                            "(expected on most headless CI runners - see "
                            "libraries/atlas-render/README.md's headless-CI decision): "
                         << error.what();
        }
    }

    // Protected accessors (rather than protected data members) keep the
    // members themselves private, satisfying cppcoreguidelines' "no
    // protected data members" check while still giving each TEST_F body the
    // access a GTest fixture is for - mirrors
    // tests/atlas-render/sdl3_frame_backend_test.cpp's own backend()/
    // mesh_id() accessor precedent exactly.
    [[nodiscard]] input::Sdl3RawSignalSource& source() { return *source_; }
    [[nodiscard]] render::Sdl3FrameBackend& frame_backend() { return *frame_backend_; }

private:
    // Declaration order matters here: window_ must outlive frame_backend_/
    // source_ (both borrow it by reference), and registry_ must outlive
    // frame_backend_ - destruction runs in reverse declaration order, so
    // this order is also the correct teardown order.
    std::optional<windowing::Sdl3SharedWindow> window_;
    std::optional<resource::ResourceRegistry> registry_;
    std::optional<render::Sdl3FrameBackend> frame_backend_;
    std::optional<input::Sdl3RawSignalSource> source_;
};

TEST_F(PresentationAppSdl3Test, RunsBoundedTicksAgainstTheRealSharedWindowBackends) {
    Argv args{"demo-host", "--ticks", "3"};
    PresentationApp<input::Sdl3RawSignalSource, render::Sdl3FrameBackend> app(
        args.argc(), args.argv(), std::move(source()), std::move(frame_backend()));

    EXPECT_EQ(app.run(), 0);
}

} // namespace
} // namespace atlas::demo
