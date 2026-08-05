// Proves §19's core claim end to end: "a button click and a keypress are
// indistinguishable to the capabilities below them." door composes
// interactable::Interactable (issue #237's generalization of this
// capability's original, door-specific build_open_control) so
// interactable_hud::build_control() can build its HUD control generically;
// door_hud now keeps only the Intent -> door::OpenDoor translation, the one
// genuinely door-specific piece. `door` itself is never modified and never
// told door_hud/interactable_hud exist (spec §5/§20 Design Rule).
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/ui/node.hpp"

#include <gtest/gtest.h>

#include "door/door.hpp"
#include "door_hud/door_hud.hpp"
#include "interactable/interactable.hpp"
#include "interactable_hud/interactable_hud.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

constexpr input::IntentId open_door_intent{"OpenDoor"};

void seed_interactable_door(SimulatedHost& host, EntityRef target) {
    host.interactable_store.set(target,
                                interactable::Interactable{
                                    .action = open_door_intent,
                                    .designator = ResourceId::from_name("text/open_door"),
                                });
}

TEST(DoorHud, ClickingTheGenericControlProducesAnOpenDoorIntentCarryingTheDoor) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    seed_interactable_door(server, target);
    const auto control = interactable_hud::build_control(server.ctx, target);
    ASSERT_TRUE(control.has_value());

    const auto intent = control->try_click(server.ctx, target);

    ASSERT_TRUE(intent.has_value());
    EXPECT_EQ(intent->id, open_door_intent);
    EXPECT_EQ(intent->entity, target);
}

TEST(DoorHud, AnIntentThatIsNotOpenDoorTranslatesToNoRequest) {
    const auto request = door_hud::to_open_door_request(
        input::Intent{.id = input::IntentId{"PickUp"}, .entity = EntityRef{9, 0}});

    EXPECT_FALSE(request.has_value());
}

TEST(DoorHud, AnOpenDoorIntentTranslatesToAnOpenDoorRequestForTheSameEntity) {
    const EntityRef target{9, 0};

    const auto request =
        door_hud::to_open_door_request(input::Intent{.id = open_door_intent, .entity = target});

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->door, target);
}

// The end-to-end proof: a click on the generically-built Node reaches
// authoritative state through the exact same Intent -> request pipeline
// hardware input uses (demo/presentation_app.hpp's own pre_tick), never a
// UI-specific shortcut into door's state.
TEST(DoorHud, ClickToRequestPipelineOpensTheDoorThroughTheOrdinaryRequestSystem) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    const auto cue = ResourceId::from_name("sfx/door/open");
    server.door_store.set(target, door::Door{.open = false, .cue = cue});
    seed_interactable_door(server, target);
    const auto control = interactable_hud::build_control(server.ctx, target);
    ASSERT_TRUE(control.has_value());

    const auto intent = control->try_click(server.ctx, target);
    ASSERT_TRUE(intent.has_value());
    const auto request = door_hud::to_open_door_request(*intent);
    ASSERT_TRUE(request.has_value());

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);
    const RequestResult result = dispatcher.dispatch(server.ctx, *request);

    ASSERT_TRUE(result.accepted);
    const auto opened_door = server.ctx.get<door::Door>(target);
    ASSERT_TRUE(opened_door.has_value());
    EXPECT_TRUE(opened_door->get().open);
}

TEST(DoorHud, ClickingAHiddenControlProducesNoIntentSoNoRequestCanFollow) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    seed_interactable_door(server, target);
    auto control = interactable_hud::build_control(server.ctx, target);
    ASSERT_TRUE(control.has_value());
    control->visible = {.value = false};

    const auto intent = control->try_click(server.ctx, target);

    EXPECT_FALSE(intent.has_value());
}

} // namespace
} // namespace atlas::demo
