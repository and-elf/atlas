#include "atlas/input/null_raw_signal_source.hpp"

#include <gtest/gtest.h>

namespace atlas::input {
namespace {

static_assert(RawSignalSource<NullRawSignalSource>);

TEST(NullRawSignalSource, PollAlwaysReturnsAnEmptyFrame) {
    NullRawSignalSource source;

    EXPECT_TRUE(source.poll().empty());
}

TEST(NullRawSignalSource, RepeatedPollsStayEmpty) {
    NullRawSignalSource source;

    const auto first = source.poll();
    const auto second = source.poll();

    EXPECT_TRUE(first.empty());
    EXPECT_TRUE(second.empty());
}

} // namespace
} // namespace atlas::input
