// Proves the end-to-end resource mechanism discussed for #146: a capability
// names a resource by setting a composed property (Door::cue, a ResourceId),
// never a bespoke "load this resource" message - a request handler mutates
// that property in place exactly like health::on_apply_damage or
// haste::on_activate_haste already do, and
// OpeningTheDoorResourceResolvesToTheRealSoundBytes proves the id it set is
// exactly the one atlas::resource::ResourceRegistry (#66/PR #118) resolves to
// the correct real bytes, without needing #55's real audio device I/O or
// #69's real GPU backend - resolution stopping at "the correct bytes were
// fetched" is the whole point being proven here.
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "door/door.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

// Set by demo/tests/CMakeLists.txt to the absolute path of demo/tests/fixtures/
// - a real packed blob on disk (demo/tests/fixtures/sound.blob), generated the
// same way as tests/atlas-resource/fixtures/'s blobs, standing in for a real
// door-open sound asset.
constexpr std::string_view fixtures_dir = DEMO_TEST_FIXTURES_DIR;

std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

TEST(Door, OpenDoorAcceptedFlipsOpenAndPublishesTheSeededCue) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    const auto cue = ResourceId::from_name("sfx/door/open");
    server.door_store.set(target, door::Door{.open = false, .cue = cue});

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);

    const RequestResult result = dispatcher.dispatch(server.ctx, door::OpenDoor{.door = target});

    ASSERT_TRUE(result.accepted);
    const auto opened_door = server.ctx.get<door::Door>(target);
    ASSERT_TRUE(opened_door.has_value());
    EXPECT_TRUE(opened_door->get().open);
    EXPECT_EQ(opened_door->get().cue, cue);
}

TEST(Door, OpenDoorRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.door_store.set(target, door::Door{.open = false, .cue = ResourceId::from_name("sfx/door/open")});

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);

    const RequestResult result = dispatcher.dispatch(client.ctx, door::OpenDoor{.door = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Door, OpenDoorRejectedWithoutADoorPropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity(); // no Door seeded

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);

    const RequestResult result = dispatcher.dispatch(server.ctx, door::OpenDoor{.door = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "door has no Door property");
}

TEST(Door, OpenDoorRejectedWhenAlreadyOpen) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.door_store.set(target, door::Door{.open = true, .cue = ResourceId::from_name("sfx/door/open")});

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);

    const RequestResult result = dispatcher.dispatch(server.ctx, door::OpenDoor{.door = target});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "door already open");
}

TEST(Door, OpeningTheDoorResourceResolvesToTheRealSoundBytes) {
    // The actual point of #146: the ResourceId the capability set via an
    // ordinary composed property (never a bespoke "load this resource"
    // message) is exactly what a real ResourceRegistry, loaded from a real
    // packed blob, resolves to the correct bytes.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    const auto cue = ResourceId::from_name("sfx/door/open");
    server.door_store.set(target, door::Door{.open = false, .cue = cue});

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);
    ASSERT_TRUE(dispatcher.dispatch(server.ctx, door::OpenDoor{.door = target}).accepted);

    const auto opened_door = server.ctx.get<door::Door>(target);
    ASSERT_TRUE(opened_door.has_value());
    const ResourceId resolved_cue = opened_door->get().cue;

    resource::ResourceRegistry registry{{{"Sound", std::filesystem::path{fixtures_dir} / "sound.blob"}}};
    const resource::Resolution resolution = registry.resolve("Sound", resolved_cue);

    ASSERT_EQ(resolution.status, resource::ResolutionStatus::Resolved);
    EXPECT_EQ(resolution.bytes, to_bytes("DOOROPENSFXBYTES"));
}

} // namespace
} // namespace atlas::demo
