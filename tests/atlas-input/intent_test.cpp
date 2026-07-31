#include "atlas/input/intent.hpp"

#include <gtest/gtest.h>

namespace atlas::input {
namespace {

TEST(IntentId, EqualityComparesByName) {
    EXPECT_EQ(IntentId{"Interact"}, IntentId{"Interact"});
    EXPECT_NE(IntentId{"Interact"}, IntentId{"MoveForward"});
}

TEST(Intent, DefaultConstructedIsTheNullIntentIdWithZeroAxis) {
    constexpr Intent intent;

    EXPECT_EQ(intent.id, IntentId{});
    EXPECT_EQ(intent.axis, 0.0F);
}

TEST(Intent, EqualityComparesIdAndAxis) {
    EXPECT_EQ((Intent{.id = IntentId{"Interact"}, .axis = 1.0F}),
              (Intent{.id = IntentId{"Interact"}, .axis = 1.0F}));
    EXPECT_NE((Intent{.id = IntentId{"Interact"}, .axis = 1.0F}),
              (Intent{.id = IntentId{"Interact"}, .axis = 0.0F}));
    EXPECT_NE((Intent{.id = IntentId{"Interact"}, .axis = 1.0F}),
              (Intent{.id = IntentId{"MoveForward"}, .axis = 1.0F}));
}

} // namespace
} // namespace atlas::input
