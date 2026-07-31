#include "atlas/input/raw_signal.hpp"
#include "atlas/input/scripted_raw_signal_source.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace atlas::input {
namespace {

TEST(RawSignalId, EqualityComparesByName) {
    EXPECT_EQ(RawSignalId{"KeyE"}, RawSignalId{"KeyE"});
    EXPECT_NE(RawSignalId{"KeyE"}, RawSignalId{"KeyW"});
}

TEST(RawSignalEvent, EqualityComparesSignalAndValue) {
    EXPECT_EQ((RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}),
              (RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}));
    EXPECT_NE((RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}),
              (RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 0.0F}));
}

TEST(ScriptedRawSignalSource, PollReturnsScriptedFramesInOrder) {
    ScriptedRawSignalSource source{{
        {RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}},
        {RawSignalEvent{.signal = RawSignalId{"KeyW"}, .value = 1.0F},
         RawSignalEvent{.signal = RawSignalId{"KeyA"}, .value = 1.0F}},
    }};

    const std::vector<RawSignalEvent> first_poll = source.poll();
    const std::vector<RawSignalEvent> second_poll = source.poll();

    ASSERT_EQ(first_poll.size(), 1U);
    EXPECT_EQ(first_poll[0].signal, RawSignalId{"KeyE"});
    ASSERT_EQ(second_poll.size(), 2U);
    EXPECT_EQ(second_poll[0].signal, RawSignalId{"KeyW"});
    EXPECT_EQ(second_poll[1].signal, RawSignalId{"KeyA"});
}

TEST(ScriptedRawSignalSource, PollPastTheScriptReturnsEmptyRatherThanThrowing) {
    ScriptedRawSignalSource source{{{RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}}}};

    (void)source.poll();
    const std::vector<RawSignalEvent> exhausted_poll = source.poll();
    const std::vector<RawSignalEvent> still_exhausted_poll = source.poll();

    EXPECT_TRUE(exhausted_poll.empty());
    EXPECT_TRUE(still_exhausted_poll.empty());
}

TEST(ScriptedRawSignalSource, EmptyScriptPollsAsEmptyImmediately) {
    ScriptedRawSignalSource source{{}};

    EXPECT_TRUE(source.poll().empty());
}

static_assert(RawSignalSource<ScriptedRawSignalSource>,
              "ScriptedRawSignalSource must satisfy the injectable raw input source seam");

} // namespace
} // namespace atlas::input
