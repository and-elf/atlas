#include "atlas/input/binding.hpp"

#include <gtest/gtest.h>

namespace atlas::input {
namespace {

TEST(InputBinding, EqualityComparesRawSignalAndIntent) {
    const InputBinding first{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}};
    const InputBinding second{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}};
    const InputBinding different_signal{.raw_signal = RawSignalId{"KeyW"}, .intent = IntentId{"Interact"}};
    const InputBinding different_intent{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"MoveForward"}};

    EXPECT_EQ(first, second);
    EXPECT_NE(first, different_signal);
    EXPECT_NE(first, different_intent);
}

} // namespace
} // namespace atlas::input
