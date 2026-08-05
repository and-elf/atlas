#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"
#include "atlas/ui/node.hpp"

#include <gtest/gtest.h>

namespace atlas::ui {
namespace {

atlas::runtime::Host make_host() {
    auto sequence = atlas::stage::StageSequence::create({atlas::stage::StageId{"Simulation"}});
    return atlas::runtime::Host{std::move(*sequence), /*has_authority=*/true};
}

TEST(Node, DefaultsToVisibleWithNoChildrenNoResourceAndNoClickable) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Node node{};

    EXPECT_TRUE(node.visible.resolve(ctx));
    EXPECT_TRUE(node.children.empty());
    EXPECT_TRUE(node.resource.resolve(ctx).is_null());
    EXPECT_FALSE(node.clickable.has_value());
}

TEST(Node, CanOwnChildNodesFormingATree) {
    Node root{};
    root.children.push_back(Node{.transform = {.x = 1.0F}});
    root.children.push_back(Node{.transform = {.x = 2.0F}});

    ASSERT_EQ(root.children.size(), 2U);
    EXPECT_FLOAT_EQ(root.children[0].transform.x, 1.0F);
    EXPECT_FLOAT_EQ(root.children[1].transform.x, 2.0F);
}

TEST(Node, VisibleRebindsToAComposedPropertyAndReResolves) {
    auto host = make_host();
    atlas::Context ctx{host};
    atlas::runtime::PropertyStore<bool> panel_open;
    panel_open.set(atlas::EntityRef{3, 0}, false);
    ctx.register_property_store(panel_open);
    const Node node{.visible = {.value = true, .bound_entity = atlas::EntityRef{3, 0}}};

    EXPECT_FALSE(node.visible.resolve(ctx));
    panel_open.set(atlas::EntityRef{3, 0}, true);
    EXPECT_TRUE(node.visible.resolve(ctx));
}

TEST(Node, ResourceReferenceBindsAnIconLikeAnyOtherBindableProperty) {
    auto host = make_host();
    atlas::Context ctx{host};
    const auto icon = atlas::ResourceId::from_name("icons/fireball");
    const Node node{.resource = {.value = icon}};

    EXPECT_EQ(node.resource.resolve(ctx), icon);
}

TEST(Node, TryClickOnAVisibleNodeWithAnEnabledClickableProducesAnEvent) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Node node{.clickable = Clickable{.intent = atlas::input::IntentId{"CastAbility"}}};

    const auto event = node.try_click(ctx, atlas::EntityRef{5, 0});

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->id, (atlas::input::IntentId{"CastAbility"}));
    EXPECT_EQ(event->entity, (atlas::EntityRef{5, 0}));
}

TEST(Node, TryClickOnANodeWithNoClickableProducesNoEvent) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Node node{};

    EXPECT_FALSE(node.try_click(ctx, atlas::EntityRef{5, 0}).has_value());
}

// A hidden node cannot be clicked even though its Clickable behavior is
// itself enabled - visibility gates interaction at the node level, not just
// the behavior level.
TEST(Node, TryClickOnAHiddenNodeProducesNoEventEvenWithAnEnabledClickable) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Node node{.visible = {.value = false}, .clickable = Clickable{}};

    EXPECT_FALSE(node.try_click(ctx, atlas::EntityRef{5, 0}).has_value());
}

TEST(Node, TryClickOnAVisibleNodeWithADisabledClickableProducesNoEvent) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Node node{.clickable = Clickable{.enabled = {.value = false}}};

    EXPECT_FALSE(node.try_click(ctx, atlas::EntityRef{5, 0}).has_value());
}

} // namespace
} // namespace atlas::ui
