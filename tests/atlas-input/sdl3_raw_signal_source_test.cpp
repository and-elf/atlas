#include "atlas/input/raw_signal.hpp"
#include "atlas/input/sdl3_raw_signal_source.hpp"
#include "atlas/windowing/sdl3_shared_window.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace atlas::input {
namespace {

static_assert(RawSignalSource<Sdl3RawSignalSource>);

bool contains_signal(const std::vector<RawSignalEvent>& events, std::string_view name) {
    return std::any_of(events.begin(), events.end(), [name](const RawSignalEvent& event) {
        return event.signal.name == name;
    });
}

// No real display/GPU needed for keyboard/mouse/gamepad state polling - this
// backend never touches SDL_GPU at all (see its own class doc comment) -
// but SDL video init still needs *some* driver. "offscreen" (SDL3's own
// software video backend) matches libraries/atlas-render's own headless-CI
// convention (tests/atlas-render/sdl3_frame_backend_test.cpp) exactly,
// though unlike that GPU-dependent backend, construction here is expected
// to always succeed in CI - there is no real-hardware dependency to skip on.
class Sdl3RawSignalSourceTest : public ::testing::Test {
protected:
    void SetUp() override { SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen"); }
};

TEST_F(Sdl3RawSignalSourceTest, ConstructionSucceedsHeadlessly) {
    EXPECT_NO_THROW({ Sdl3RawSignalSource source; });
}

TEST_F(Sdl3RawSignalSourceTest, PollAlwaysReportsCurrentMousePositionAsContinuousSignals) {
    Sdl3RawSignalSource source;

    const std::vector<RawSignalEvent> events = source.poll();

    EXPECT_TRUE(contains_signal(events, "MouseX"));
    EXPECT_TRUE(contains_signal(events, "MouseY"));
}

// SDL_WarpMouseGlobal (a genuinely public SDL3 API, unlike keyboard/mouse-
// button state - see the comment on the keyboard test below) was tried here
// to verify poll() reports a real, moved position rather than just "some
// position" - but SDL3's "offscreen" video driver (this test suite's
// headless-CI choice, same as tests/atlas-render's) has no real display to
// warp a cursor on, so SDL_WarpMouseGlobal itself returns false under it.
// Confirmed empirically, not assumed. Left as presence-only coverage
// (above) rather than a broken value-assertion test.

TEST_F(Sdl3RawSignalSourceTest, PollNeverReportsAMouseButtonWhenNoneIsPressed) {
    Sdl3RawSignalSource source;

    const std::vector<RawSignalEvent> events = source.poll();

    EXPECT_FALSE(contains_signal(events, "MouseLeft"));
    EXPECT_FALSE(contains_signal(events, "MouseRight"));
    EXPECT_FALSE(contains_signal(events, "MouseMiddle"));
}

TEST_F(Sdl3RawSignalSourceTest, PollReportsNoGamepadSignalsWhenNoneIsConnected) {
    // This sandbox (and every headless CI runner) has no physical
    // controller attached - proves the "no gamepad connected" path never
    // crashes and simply reports nothing gamepad-related, rather than
    // exhaustively exercising real controller input (which needs #170's
    // demo-integration-level hardware access, not this library's own tests).
    Sdl3RawSignalSource source;

    const std::vector<RawSignalEvent> events = source.poll();

    EXPECT_FALSE(contains_signal(events, "GamepadButtonSouth"));
    EXPECT_FALSE(contains_signal(events, "GamepadLeftStickX"));
}

// A genuine "key is currently down" test is not achievable through SDL3's
// public API in headless CI: the internal keyboard-state array poll() reads
// (SDL_GetKeyboardState) is only ever updated by SDL_SendKeyboardKey, which
// is an SDL-internal function (defined in src/events/SDL_keyboard.c, never
// declared in any public SDL3/*.h header) that real platform video backends
// call when translating genuine OS input - pushing a synthetic
// SDL_EVENT_KEY_DOWN via the public SDL_PushEvent does not reach it, so it
// never updates the state array poll() actually reads. Confirmed by reading
// SDL3's own fetched source (build/*/​_deps/sdl3-src/src/events/SDL_keyboard.c),
// not assumed. This is a genuine, documented testing limitation - matching
// this project's practice of stating a real gap plainly rather than forcing
// a synthetic-injection test that would silently test nothing - not a
// design defect in this backend's actual production behavior, which does
// see real key state correctly (a real OS key press does reach
// SDL_SendKeyboardKey through SDL's own video backend).
TEST_F(Sdl3RawSignalSourceTest, KeyboardScanLoopRunsEveryPollWithoutCrashingRegardlessOfState) {
    Sdl3RawSignalSource source;

    const std::vector<RawSignalEvent> events = source.poll();

    // No key in this backend's curated table is down in this environment,
    // so none should be reported - proves the scan loop's bounds-checked
    // lookup runs cleanly against a real SDL_GetKeyboardState() array every
    // poll, even though the "currently pressed" branch itself can't be
    // exercised here (see comment above).
    EXPECT_FALSE(contains_signal(events, "KeyE"));
    EXPECT_FALSE(contains_signal(events, "KeyW"));
}

// Issue #174: the shared-window alternate constructor - proves
// Sdl3RawSignalSource can borrow an already-created windowing::Sdl3SharedWindow
// (rather than creating its own window) and still poll() successfully, and
// that destroying it leaves the still-alive Sdl3SharedWindow's own handle()
// untouched (this backend's shared-window destructor must never destroy a
// window it doesn't own).
TEST(Sdl3RawSignalSourceSharedWindow, PollSucceedsAgainstASharedWindowAndLeavesItUntouchedOnDestruction) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

    windowing::Sdl3SharedWindow shared_window{"atlas-input-tests-shared", 64, 64, SDL_WINDOW_HIDDEN};
    SDL_Window* const original_handle = shared_window.handle();

    {
        Sdl3RawSignalSource source{shared_window};
        const std::vector<RawSignalEvent> events = source.poll();
        EXPECT_TRUE(contains_signal(events, "MouseX"));
    }

    EXPECT_EQ(shared_window.handle(), original_handle);
}

TEST(Sdl3RawSignalSourceConstruction, FailureReportsSdlErrorTextInTheException) {
    // A deterministic construction failure, independent of the sandbox's own
    // hardware - mirrors Sdl3FrameBackendConstruction's own
    // FailureReportsSdlErrorTextInTheException test exactly (issue #151):
    // an invalid SDL_HINT_VIDEO_DRIVER value makes SDL_Init(SDL_INIT_VIDEO)
    // itself fail, giving real coverage of this backend's throwing
    // constructor path without depending on any environment quirk.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "this-driver-does-not-exist");

    try {
        Sdl3RawSignalSource source;
        FAIL() << "expected SDL_Init to fail for an invalid video driver hint";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string_view{error.what()}.find("SDL_Init"), std::string_view::npos);
    }
}

TEST_F(Sdl3RawSignalSourceTest, MoveConstructionTransfersOwnershipAndLeavesSourceUsable) {
    Sdl3RawSignalSource original;

    Sdl3RawSignalSource moved(std::move(original));

    EXPECT_NO_FATAL_FAILURE({ static_cast<void>(moved.poll()); });
}

TEST_F(Sdl3RawSignalSourceTest, MoveAssignmentTransfersOwnershipAndLeavesSourceUsable) {
    Sdl3RawSignalSource first;
    Sdl3RawSignalSource second;

    second = std::move(first);

    EXPECT_NO_FATAL_FAILURE({ static_cast<void>(second.poll()); });
}

} // namespace
} // namespace atlas::input
