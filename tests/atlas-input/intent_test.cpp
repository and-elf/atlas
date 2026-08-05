#include "atlas/input/intent.hpp"

#include <gtest/gtest.h>

namespace atlas::input {
namespace {

TEST(IntentId, EqualityComparesByName) {
    EXPECT_EQ(IntentId{"Interact"}, IntentId{"Interact"});
    EXPECT_NE(IntentId{"Interact"}, IntentId{"MoveForward"});
}

TEST(Intent, DefaultConstructedIsTheNullIntentIdWithNullEntityAndZeroAxis) {
    constexpr Intent intent;

    EXPECT_EQ(intent.id, IntentId{});
    EXPECT_TRUE(intent.entity.is_null());
    EXPECT_EQ(intent.axis, 0.0F);
}

TEST(Intent, EqualityComparesIdEntityAndAxis) {
    EXPECT_EQ((Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{7, 0}, .axis = 1.0F}),
              (Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{7, 0}, .axis = 1.0F}));
    EXPECT_NE((Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{7, 0}, .axis = 1.0F}),
              (Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{7, 0}, .axis = 0.0F}));
    EXPECT_NE((Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{7, 0}, .axis = 1.0F}),
              (Intent{.id = IntentId{"MoveForward"}, .entity = atlas::EntityRef{7, 0}, .axis = 1.0F}));
    EXPECT_NE((Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{7, 0}, .axis = 1.0F}),
              (Intent{.id = IntentId{"Interact"}, .entity = atlas::EntityRef{8, 0}, .axis = 1.0F}));
}

} // namespace
} // namespace atlas::input
