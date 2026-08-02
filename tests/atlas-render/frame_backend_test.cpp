#include "atlas/render/frame_backend.hpp"
#include "atlas/render/null_frame_backend.hpp"

#include <gtest/gtest.h>

namespace atlas::render {
namespace {

static_assert(FrameBackend<NullFrameBackend>);

TEST(NullFrameBackend, LastCompletedTickIsNulloptBeforeAnySubmit) {
    const NullFrameBackend backend;

    EXPECT_FALSE(backend.last_completed_tick().has_value());
}

TEST(NullFrameBackend, SubmitRecordsTheFramesTickAsCompleted) {
    NullFrameBackend backend;
    const Frame frame{.tick = core::Time{.ticks = 7}, .draw_commands = {}};

    backend.submit(frame);

    ASSERT_TRUE(backend.last_completed_tick().has_value());
    EXPECT_EQ(*backend.last_completed_tick(), (core::Time{.ticks = 7}));
}

TEST(NullFrameBackend, RepeatedSubmitsTrackTheLatestTick) {
    NullFrameBackend backend;

    backend.submit(Frame{.tick = core::Time{.ticks = 5}, .draw_commands = {}});
    backend.submit(Frame{.tick = core::Time{.ticks = 10}, .draw_commands = {}});

    ASSERT_TRUE(backend.last_completed_tick().has_value());
    EXPECT_EQ(*backend.last_completed_tick(), (core::Time{.ticks = 10}));
}

TEST(NullFrameBackend, SubmitIgnoresDrawCommandContentsWithoutFailing) {
    // "Does nothing with a Frame's draw commands" proven by handing it a
    // genuinely non-empty command list and confirming only tick tracking
    // is observable afterward - not just exercised with an always-empty
    // Frame, which would never distinguish "ignores commands" from "would
    // break given real ones."
    NullFrameBackend backend;
    const Frame frame{
        .tick = core::Time{.ticks = 3},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{}, .transform = {}, .mesh = {}, .material = {}},
                DrawCommand{.entity = EntityRef{}, .transform = {}, .mesh = {}, .material = {}},
            },
    };

    backend.submit(frame);

    ASSERT_TRUE(backend.last_completed_tick().has_value());
    EXPECT_EQ(*backend.last_completed_tick(), (core::Time{.ticks = 3}));
}

} // namespace
} // namespace atlas::render
