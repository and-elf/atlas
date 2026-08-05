// lootable is the second Interactable consumer (issue #237): a genuinely
// different action shape from door's self-contained toggle - PickUp names
// both the clicked item and a separate collector entity, and a collected
// item rejects a second PickUp rather than toggling back and forth the way
// a door opens/closes. Mirrors door_test.cpp's own shape/coverage exactly.
#include "atlas/request/dispatch.hpp"

#include <gtest/gtest.h>

#include "lootable/lootable.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Lootable, PickUpAcceptedMarksTheItemCollectedAndPublishesPickedUp) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef item = server.host.create_entity();
    const EntityRef collector = server.host.create_entity();
    server.lootable_store.set(item, lootable::Lootable{.collected = false});

    request::Dispatcher<lootable::PickUp> dispatcher;
    dispatcher.register_handler(lootable::on_pick_up);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, lootable::PickUp{.item = item, .collector = collector});

    ASSERT_TRUE(result.accepted);
    const auto looted = server.ctx.get<lootable::Lootable>(item);
    ASSERT_TRUE(looted.has_value());
    EXPECT_TRUE(looted->get().collected);
}

TEST(Lootable, PickUpRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef item = client.host.create_entity();
    const EntityRef collector = client.host.create_entity();
    client.lootable_store.set(item, lootable::Lootable{.collected = false});

    request::Dispatcher<lootable::PickUp> dispatcher;
    dispatcher.register_handler(lootable::on_pick_up);

    const RequestResult result =
        dispatcher.dispatch(client.ctx, lootable::PickUp{.item = item, .collector = collector});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Lootable, PickUpRejectedWithoutALootablePropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef item = server.host.create_entity(); // no Lootable seeded
    const EntityRef collector = server.host.create_entity();

    request::Dispatcher<lootable::PickUp> dispatcher;
    dispatcher.register_handler(lootable::on_pick_up);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, lootable::PickUp{.item = item, .collector = collector});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "item has no Lootable property");
}

TEST(Lootable, PickUpRejectedWhenAlreadyCollected) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef item = server.host.create_entity();
    const EntityRef collector = server.host.create_entity();
    server.lootable_store.set(item, lootable::Lootable{.collected = true});

    request::Dispatcher<lootable::PickUp> dispatcher;
    dispatcher.register_handler(lootable::on_pick_up);

    const RequestResult result =
        dispatcher.dispatch(server.ctx, lootable::PickUp{.item = item, .collector = collector});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "item already collected");
}

} // namespace
} // namespace atlas::demo
