// interactable_hud is the generic replacement for door_hud's original
// build_open_control (issue #237): one function that builds a real
// atlas::ui::Node for *any* entity composing interactable::Interactable,
// rather than one hand-written Node-building function per entity type.
// door_hud/lootable_hud each keep only their own small Intent -> request
// translator - see door_hud_test.cpp/lootable_hud_test.cpp for the
// end-to-end proof this feeds into.
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

#include "interactable/interactable.hpp"
#include "interactable_hud/interactable_hud.hpp"

namespace atlas::demo {
namespace {

runtime::Host make_host() {
    auto sequence = stage::StageSequence::create({stage::StageId{"Simulation"}});
    return runtime::Host{std::move(*sequence), /*has_authority=*/true};
}

TEST(InteractableHud, BuildsANodeCarryingTheEntitysOwnActionAndDesignator) {
    auto host = make_host();
    Context ctx{host};
    runtime::PropertyStore<interactable::Interactable> store;
    const EntityRef door{1, 0};
    const auto designator = ResourceId::from_name("text/open_door");
    store.set(door,
              interactable::Interactable{.action = input::IntentId{"OpenDoor"}, .designator = designator});
    ctx.register_property_store(store);

    const auto node = interactable_hud::build_control(ctx, door);

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->resource.resolve(ctx), designator);
    ASSERT_TRUE(node->clickable.has_value());
}

TEST(InteractableHud, ClickingTheBuiltNodeProducesTheEntitysOwnIntent) {
    auto host = make_host();
    Context ctx{host};
    runtime::PropertyStore<interactable::Interactable> store;
    const EntityRef door{1, 0};
    store.set(door,
              interactable::Interactable{.action = input::IntentId{"OpenDoor"},
                                         .designator = ResourceId::from_name("text/open_door")});
    ctx.register_property_store(store);
    const auto node = interactable_hud::build_control(ctx, door);
    ASSERT_TRUE(node.has_value());

    const auto intent = node->try_click(ctx, door);

    ASSERT_TRUE(intent.has_value());
    EXPECT_EQ(intent->id, (input::IntentId{"OpenDoor"}));
    EXPECT_EQ(intent->entity, door);
}

TEST(InteractableHud, AnEntityWithNoInteractablePropertyProducesNoControl) {
    auto host = make_host();
    Context ctx{host};
    runtime::PropertyStore<interactable::Interactable> store;
    ctx.register_property_store(store);
    const EntityRef nothing{2, 0};

    const auto node = interactable_hud::build_control(ctx, nothing);

    EXPECT_FALSE(node.has_value());
}

} // namespace
} // namespace atlas::demo
