#include "atlas/ui/transform.hpp"

#include <gtest/gtest.h>

namespace atlas::ui {
namespace {

TEST(Transform2D, DefaultsToZeroPositionSizeAndRotation) {
    const Transform2D transform;

    EXPECT_FLOAT_EQ(transform.x, 0.0F);
    EXPECT_FLOAT_EQ(transform.y, 0.0F);
    EXPECT_FLOAT_EQ(transform.width, 0.0F);
    EXPECT_FLOAT_EQ(transform.height, 0.0F);
    EXPECT_FLOAT_EQ(transform.rotation_degrees, 0.0F);
}

TEST(Transform2D, EqualityComparesAllFields) {
    const Transform2D a{.x = 1.0F, .y = 2.0F, .width = 3.0F, .height = 4.0F, .rotation_degrees = 5.0F};
    const Transform2D b{.x = 1.0F, .y = 2.0F, .width = 3.0F, .height = 4.0F, .rotation_degrees = 5.0F};
    const Transform2D c{.x = 9.0F, .y = 2.0F, .width = 3.0F, .height = 4.0F, .rotation_degrees = 5.0F};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

} // namespace
} // namespace atlas::ui
