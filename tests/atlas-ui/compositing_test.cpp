#include "atlas/ui/compositing.hpp"

#include <gtest/gtest.h>

namespace atlas::ui {
namespace {

// Spec §19, Compositing Layers: "three fixed macro-layers, always in this
// order": World -> HUD -> Menu. This library implements only the ordering
// concept (the enum and its natural integer ordering) - not the compositing
// engine itself, which is out of scope this pass (see library README).
TEST(Layer, OrdersWorldBeforeHudBeforeMenu) {
    EXPECT_LT(Layer::World, Layer::Hud);
    EXPECT_LT(Layer::Hud, Layer::Menu);
    EXPECT_LT(Layer::World, Layer::Menu);
}

} // namespace
} // namespace atlas::ui
