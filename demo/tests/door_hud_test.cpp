// Proves §19's core claim end to end: "a button click and a keypress are
// indistinguishable to the capabilities below them." door_hud composes a
// Node (spec §19, Minimum UI Contract) whose Clickable produces an ordinary
// atlas::input::Intent - the same type IntentRouter produces from hardware
// input (see tests/atlas-input/intent_router_test.cpp) - and this test
// drives that Intent through door_hud's own translation into a real
// door::OpenDoor request, dispatched exactly like door_test.cpp's own
// hand-issued requests. `door` itself is never modified and never told
// door_hud exists (spec §5/§20 Design Rule).
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/ui/node.hpp"

#include <gtest/gtest.h>

#include "door/door.hpp"
#include "door_hud/door_hud.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(DoorHud, ClickingTheOpenControlProducesAnOpenDoorIntentCarryingTheDoor) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    const ui::Node control = door_hud::build_open_control();

    const auto intent = control.try_click(server.ctx, target);

    ASSERT_TRUE(intent.has_value());
    EXPECT_EQ(intent->id, (input::IntentId{"OpenDoor"}));
    EXPECT_EQ(intent->entity, target);
}

TEST(DoorHud, AnIntentThatIsNotOpenDoorTranslatesToNoRequest) {
    const auto request = door_hud::to_open_door_request(
        input::Intent{.id = input::IntentId{"CastAbility"}, .entity = EntityRef{9, 0}});

    EXPECT_FALSE(request.has_value());
}

TEST(DoorHud, AnOpenDoorIntentTranslatesToAnOpenDoorRequestForTheSameEntity) {
    const EntityRef target{9, 0};

    const auto request =
        door_hud::to_open_door_request(input::Intent{.id = input::IntentId{"OpenDoor"}, .entity = target});

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->door, target);
}

// The end-to-end proof: a click on the composed Node reaches authoritative
// state through the exact same Intent -> request pipeline hardware input
// uses (demo/presentation_app.hpp's own pre_tick), never a UI-specific
// shortcut into door's state.
TEST(DoorHud, ClickToRequestPipelineOpensTheDoorThroughTheOrdinaryRequestSystem) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    const auto cue = ResourceId::from_name("sfx/door/open");
    server.door_store.set(target, door::Door{.open = false, .cue = cue});
    const ui::Node control = door_hud::build_open_control();

    const auto intent = control.try_click(server.ctx, target);
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
    ui::Node control = door_hud::build_open_control();
    control.visible = {.value = false};

    const auto intent = control.try_click(server.ctx, target);

    EXPECT_FALSE(intent.has_value());
}

} // namespace
} // namespace atlas::demo
