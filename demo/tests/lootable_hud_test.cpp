// The second Interactable consumer (issue #237), proving interactable_hud's
// generic Node-building and the Intent -> request translation pattern both
// generalize beyond door: lootable composes interactable::Interactable
// (action = "PickUp") exactly like door composes it (action = "OpenDoor"),
// and interactable_hud::build_control() builds the same kind of Node for
// either, with no door/lootable-specific code in interactable_hud at all.
// `lootable` itself is never modified and never told
// lootable_hud/interactable_hud exist (spec §5/§20 Design Rule).
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/ui/node.hpp"

#include <gtest/gtest.h>

#include "interactable/interactable.hpp"
#include "interactable_hud/interactable_hud.hpp"
#include "lootable/lootable.hpp"
#include "lootable_hud/lootable_hud.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

constexpr input::IntentId pick_up_intent{"PickUp"};

void seed_interactable_item(SimulatedHost& host, EntityRef item) {
    host.interactable_store.set(item,
                                interactable::Interactable{
                                    .action = pick_up_intent,
                                    .designator = ResourceId::from_name("text/pick_up"),
                                });
}

TEST(LootableHud, ClickingTheGenericControlProducesAPickUpIntentCarryingTheItem) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef item = server.host.create_entity();
    seed_interactable_item(server, item);
    const auto control = interactable_hud::build_control(server.ctx, item);
    ASSERT_TRUE(control.has_value());

    const auto intent = control->try_click(server.ctx, item);

    ASSERT_TRUE(intent.has_value());
    EXPECT_EQ(intent->id, pick_up_intent);
    EXPECT_EQ(intent->entity, item);
}

TEST(LootableHud, AnIntentThatIsNotPickUpTranslatesToNoRequest) {
    const EntityRef collector{9, 0};

    const auto request = lootable_hud::to_pick_up_request(
        input::Intent{.id = input::IntentId{"OpenDoor"}, .entity = EntityRef{5, 0}}, collector);

    EXPECT_FALSE(request.has_value());
}

TEST(LootableHud, APickUpIntentTranslatesToAPickUpRequestNamingTheClickedItemAndSuppliedCollector) {
    const EntityRef item{5, 0};
    const EntityRef collector{9, 0};

    const auto request =
        lootable_hud::to_pick_up_request(input::Intent{.id = pick_up_intent, .entity = item}, collector);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->item, item);
    EXPECT_EQ(request->collector, collector);
}

// The end-to-end proof, mirroring door_hud_test.cpp's own
// ClickToRequestPipelineOpensTheDoorThroughTheOrdinaryRequestSystem: a click
// on the generically-built Node reaches authoritative state through the
// same Intent -> request pipeline, this time for a completely different
// request shape (PickUp names two entities; OpenDoor names one).
TEST(LootableHud, ClickToRequestPipelineCollectsTheItemThroughTheOrdinaryRequestSystem) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef item = server.host.create_entity();
    const EntityRef collector = server.host.create_entity();
    server.lootable_store.set(item, lootable::Lootable{.collected = false});
    seed_interactable_item(server, item);
    const auto control = interactable_hud::build_control(server.ctx, item);
    ASSERT_TRUE(control.has_value());

    const auto intent = control->try_click(server.ctx, item);
    ASSERT_TRUE(intent.has_value());
    const auto request = lootable_hud::to_pick_up_request(*intent, collector);
    ASSERT_TRUE(request.has_value());

    request::Dispatcher<lootable::PickUp> dispatcher;
    dispatcher.register_handler(lootable::on_pick_up);
    const RequestResult result = dispatcher.dispatch(server.ctx, *request);

    ASSERT_TRUE(result.accepted);
    const auto looted = server.ctx.get<lootable::Lootable>(item);
    ASSERT_TRUE(looted.has_value());
    EXPECT_TRUE(looted->get().collected);
}

} // namespace
} // namespace atlas::demo
