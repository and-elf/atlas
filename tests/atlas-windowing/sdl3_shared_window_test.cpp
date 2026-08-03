#include "atlas/windowing/sdl3_shared_window.hpp"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace atlas::windowing {
namespace {

// SDL_HINT_VIDEO_DRIVER forced to "offscreen" (not "dummy") for the exact
// reason documented at length in
// tests/atlas-render/sdl3_frame_backend_test.cpp's own top-of-file comment
// and tests/atlas-input/sdl3_raw_signal_source_test.cpp's SetUp() - windowing
// alone (no GPU/Vulkan surface involved here) succeeds under either driver,
// but "offscreen" is this project's established headless-CI convention for
// every real SDL3-backed test, so this file follows it too rather than
// introducing a second one.
class Sdl3SharedWindowTest : public ::testing::Test {
protected:
    void SetUp() override { SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen"); }
};

TEST_F(Sdl3SharedWindowTest, ConstructionSucceedsHeadlesslyAndReportsANonNullHandle) {
    const Sdl3SharedWindow window{"atlas-windowing-tests", 64, 64, SDL_WINDOW_HIDDEN};

    EXPECT_NE(window.handle(), nullptr);
}

TEST_F(Sdl3SharedWindowTest, MoveConstructionTransfersOwnershipAndLeavesSourceHarmlessToDestroy) {
    Sdl3SharedWindow window{"atlas-windowing-tests", 64, 64, SDL_WINDOW_HIDDEN};
    SDL_Window* const original_handle = window.handle();

    const Sdl3SharedWindow moved{std::move(window)};

    EXPECT_EQ(moved.handle(), original_handle);

    // The moved-from instance's destructor must not double-destroy the
    // window / double-call SDL_Quit() - reaching this line without a crash
    // (ASan/UBSan enabled in the debug preset) is the assertion.
}

TEST_F(Sdl3SharedWindowTest, MoveAssignmentTransfersOwnershipAndLeavesSourceHarmlessToDestroy) {
    Sdl3SharedWindow window{"atlas-windowing-tests", 64, 64, SDL_WINDOW_HIDDEN};
    SDL_Window* const original_handle = window.handle();

    Sdl3SharedWindow other{"atlas-windowing-tests-other", 32, 32, SDL_WINDOW_HIDDEN};
    other = std::move(window);

    EXPECT_EQ(other.handle(), original_handle);
}

TEST(Sdl3SharedWindowConstruction, FailureReportsSdlErrorTextInTheException) {
    // Forces a real, deterministic failure rather than relying on this
    // sandbox happening to lack some resource - mirrors
    // Sdl3FrameBackendConstruction/Sdl3RawSignalSourceConstruction's own
    // "invalid video driver" test.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "this-video-driver-does-not-exist");

    try {
        const Sdl3SharedWindow window{"atlas-windowing-tests", 64, 64, SDL_WINDOW_HIDDEN};
        FAIL() << "expected Sdl3SharedWindow construction to throw with an invalid video driver";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_FALSE(message.empty());
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
}

} // namespace
} // namespace atlas::windowing
