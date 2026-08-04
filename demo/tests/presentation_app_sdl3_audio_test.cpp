#include "atlas/audio/decode_cache.hpp"
#include "atlas/audio/sdl3_audio_backend.hpp"
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
#include <string_view>
#include <unordered_map>
#include <vector>

#include "presentation_app.hpp"

namespace atlas::demo {
namespace {

// issue #201: proves PresentationApp actually composes with the real
// Sdl3AudioBackend (not just NullAudioBackend/RecordingAudioBackend, both
// default-constructible) end to end, alongside the real Sdl3RawSignalSource/
// Sdl3FrameBackend composition presentation_app_sdl3_test.cpp already
// proves - main.cpp's own ATLAS_DEMO_REAL_AUDIO_BACKEND branch already
// proves this compiles/links at build time (this CMake target only exists
// in that same three-way-real configuration), this proves it actually runs.
//
// Mirrors PresentationAppSdl3Test's own SetUp()/GTEST_SKIP()-on-
// construction-failure pattern exactly (including the "offscreen" video
// driver hint for the render/input half - see that file's own comment for
// why "offscreen", not "dummy"). The audio half additionally forces
// SDL_HINT_AUDIO_DRIVER to "dummy" here via SDL_SetHint - this test binary
// has no earlier test that initializes SDL's audio subsystem, so a plain
// (non-SDL_HINT_OVERRIDE-priority) hint set before Sdl3AudioBackend's own
// SDL_InitSubSystem(SDL_INIT_AUDIO) call reliably wins; unlike
// tests/atlas-audio/sdl3_audio_backend_test.cpp's own construction-*failure*
// test, nothing here needs the SDL_HINT_OVERRIDE priority workaround that
// file's own comment explains.
class PresentationAppSdl3AudioTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");

        try {
            window_.emplace("atlas-demo-tests-audio", 64, 64, SDL_WINDOW_HIDDEN);
            // Real Mesh/Texture/Sound blobs (issue #200/#201), checked into
            // demo/resources/ - DEMO_RESOURCES_DIR is defined unconditionally
            // for demo-tests (see demo/tests/CMakeLists.txt's own comment).
            registry_.emplace(std::unordered_map<std::string, std::filesystem::path>{
                {"Mesh", std::filesystem::path{k_resources_dir} / "Mesh.blob"},
                {"Texture", std::filesystem::path{k_resources_dir} / "Texture.blob"},
                {"Sound", std::filesystem::path{k_resources_dir} / "Sound.blob"},
            });
            frame_backend_.emplace(*registry_, *window_);
            source_.emplace(*window_);
            decode_cache_.emplace(*registry_, "Sound");
            audio_backend_.emplace(*decode_cache_);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << "No real SDL3/SDL_GPU-capable backend and/or audio device available "
                            "in this environment (expected on most headless CI runners - see "
                            "libraries/atlas-render/README.md's headless-CI decision): "
                         << error.what();
        }
    }

    // Protected accessors (rather than protected data members), matching
    // PresentationAppSdl3Test's own precedent exactly.
    [[nodiscard]] input::Sdl3RawSignalSource& source() { return *source_; }
    [[nodiscard]] render::Sdl3FrameBackend& frame_backend() { return *frame_backend_; }
    [[nodiscard]] audio::Sdl3AudioBackend& audio_backend() { return *audio_backend_; }

private:
    static constexpr std::string_view k_resources_dir = DEMO_RESOURCES_DIR;

    // Declaration order matters here: window_ must outlive frame_backend_/
    // source_ (both borrow it by reference); registry_ must outlive
    // frame_backend_ and decode_cache_; decode_cache_ must outlive
    // audio_backend_ (Sdl3AudioBackend's own "decode_cache must outlive this
    // backend" contract) - destruction runs in reverse declaration order, so
    // this order is also the correct teardown order (audio_backend_ first,
    // decode_cache_ next, then source_/frame_backend_, then registry_, then
    // window_ last).
    std::optional<windowing::Sdl3SharedWindow> window_;
    std::optional<resource::ResourceRegistry> registry_;
    std::optional<render::Sdl3FrameBackend> frame_backend_;
    std::optional<input::Sdl3RawSignalSource> source_;
    std::optional<audio::DecodeCache> decode_cache_;
    std::optional<audio::Sdl3AudioBackend> audio_backend_;
};

// A real argv-shaped array (argv[argc] must be a null pointer, the same
// contract main()'s own argv has) - mirrors presentation_app_sdl3_test.cpp's
// own Argv, kept as a separate small copy here rather than shared test
// scaffolding for one call site.
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

TEST_F(PresentationAppSdl3AudioTest, RunsBoundedTicksAgainstTheRealSharedWindowAndAudioBackends) {
    Argv args{"demo-host", "--ticks", "3"};
    PresentationApp<input::Sdl3RawSignalSource, render::Sdl3FrameBackend, audio::Sdl3AudioBackend> app(
        args.argc(),
        args.argv(),
        std::move(source()),
        std::move(frame_backend()),
        std::move(audio_backend()));

    EXPECT_EQ(app.run(), 0);
}

} // namespace
} // namespace atlas::demo
