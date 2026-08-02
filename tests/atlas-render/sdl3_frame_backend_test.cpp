#include "atlas/render/frame_backend.hpp"
#include "atlas/render/sdl3_frame_backend.hpp"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>

namespace atlas::render {
namespace {

static_assert(FrameBackend<Sdl3FrameBackend>);

// Most CI runners (this sandbox included - no /dev/dri, no Vulkan ICD
// installed) have no real GPU or display hardware, which is exactly why
// issue #148 built NullFrameBackend in the first place (see this library's
// README, "headless-CI decision"). Every fixture-based test below attempts
// *real* SDL3 window + SDL_GPU device creation in SetUp() and, on failure,
// calls GTEST_SKIP() with the thrown exception's message rather than
// treating it as a test failure - the constructor throwing
// std::runtime_error is itself the behavior under test on a machine like
// this one. SDL_HINT_VIDEO_DRIVER is forced to "dummy" first so windowing
// itself succeeds headlessly (no DISPLAY needed) - GPU device creation is
// then the part actually expected to fail here, absent a Vulkan/Metal/D3D12
// driver, giving line coverage over the real success path on any machine
// that does have one.
class Sdl3FrameBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        try {
            backend_.emplace("atlas-render-tests", 64, 64, SDL_WINDOW_HIDDEN);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                            "(expected on most headless CI runners - see "
                            "libraries/atlas-render/README.md's headless-CI decision): "
                         << error.what();
        }
    }

    // Polls last_completed_tick() up to max_attempts times, returning as
    // soon as it reports a value - never assumes instant completion (that
    // is the entire point of a real GPU fence over NullFrameBackend's
    // "instantly complete" shortcut), and never spins forever if a driver
    // somehow never signals.
    std::optional<core::Time> poll_until_completed(int max_attempts = 1000) {
        std::optional<core::Time> completed;
        for (int attempt = 0; attempt < max_attempts && !completed.has_value(); ++attempt) {
            completed = backend().last_completed_tick();
        }
        return completed;
    }

    // A protected accessor (rather than a protected data member) keeps
    // backend_ itself private, satisfying cppcoreguidelines' "no protected
    // data members" check while still giving every TEST_F body below the
    // access a GTest fixture is for.
    Sdl3FrameBackend& backend() { return *backend_; }

private:
    std::optional<Sdl3FrameBackend> backend_;
};

TEST_F(Sdl3FrameBackendTest, LastCompletedTickIsNulloptBeforeAnySubmit) {
    EXPECT_FALSE(backend().last_completed_tick().has_value());
}

TEST_F(Sdl3FrameBackendTest, SubmitDoesNotThrowAndEventuallyReportsTheTickCompleted) {
    const Frame frame{.tick = core::Time{.ticks = 1}, .draw_commands = {}};
    EXPECT_NO_THROW(backend().submit(frame));

    const std::optional<core::Time> completed = poll_until_completed();

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 1}));
}

TEST_F(Sdl3FrameBackendTest, RepeatedSubmitsEventuallyTrackTheLatestCompletedTick) {
    backend().submit(Frame{.tick = core::Time{.ticks = 1}, .draw_commands = {}});
    backend().submit(Frame{.tick = core::Time{.ticks = 2}, .draw_commands = {}});

    const std::optional<core::Time> completed = poll_until_completed();

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 2}));
}

TEST_F(Sdl3FrameBackendTest, SubmitIgnoresDrawCommandContentsWithoutFailing) {
    // Mirrors NullFrameBackend's own equivalent test (frame_backend_test.cpp):
    // a genuinely non-empty draw command list must not change this backend's
    // behavior, since Frame::draw_commands is deliberately unused this round
    // (see this type's class doc comment).
    const Frame frame{
        .tick = core::Time{.ticks = 3},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{}, .transform = {}, .mesh = {}, .material = {}},
            },
    };

    EXPECT_NO_THROW(backend().submit(frame));
}

TEST_F(Sdl3FrameBackendTest, MoveConstructionTransfersOwnershipAndLeavesSourceHarmlessToDestroy) {
    backend().submit(Frame{.tick = core::Time{.ticks = 5}, .draw_commands = {}});

    Sdl3FrameBackend moved{std::move(backend())};

    std::optional<core::Time> completed;
    for (int attempt = 0; attempt < 1000 && !completed.has_value(); ++attempt) {
        completed = moved.last_completed_tick();
    }
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 5}));

    // The moved-from instance's destructor must not double-release SDL /
    // the GPU device it no longer owns - reaching this line without a
    // crash (ASan/UBSan enabled in the debug preset) is the assertion.
}

TEST(Sdl3FrameBackendConstruction, FailureReportsSdlErrorTextInTheException) {
    // Forces a real failure deterministically, rather than relying on this
    // sandbox happening to have no GPU: SDL_HINT_VIDEO_DRIVER set to a
    // driver name that does not exist makes SDL_Init itself fail.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "this-video-driver-does-not-exist");

    try {
        const Sdl3FrameBackend backend{"atlas-render-tests", 64, 64, SDL_WINDOW_HIDDEN};
        FAIL() << "expected Sdl3FrameBackend construction to throw with an invalid video driver";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_FALSE(message.empty());
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
}

} // namespace
} // namespace atlas::render
