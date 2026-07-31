#include "atlas/cgen/host_composition.hpp"
#include "atlas/cgen/host_composition_writer.hpp"

#include <gtest/gtest.h>
#include <string>

namespace atlas::cgen {
namespace {

TEST(GenerateHostComposition, EmitsAPropertyStoreMemberAndRegistrationPerProperty) {
    const HostComposition composition{
        .host_name = "GameplayClient",
        .ordered_capabilities =
            {
                Manifest{
                    .capability_name = "movement",
                    .depends_on = {"entity"},
                    .consumes = {},
                    .properties = {StructDecl{.name = "Position",
                                              .fields = {Field{.name = "x", .type = "float"},
                                                         Field{.name = "y", .type = "float"}},
                                              .composition = std::nullopt},
                                   StructDecl{.name = "MovementSpeed",
                                              .fields = {Field{.name = "base", .type = "float"}},
                                              .composition = "Multiplicative"}},
                    .requests = {},
                    .events = {},
                },
            },
    };

    const std::string output =
        generate_host_composition(composition, "gameplay_client.host.hpp", "gameplay_client.host.yaml");

    EXPECT_NE(output.find("// GENERATED — gameplay_client.host.hpp"), std::string::npos);
    EXPECT_NE(output.find("Source: gameplay_client.host.yaml — do not hand-edit."), std::string::npos);
    EXPECT_NE(output.find("namespace atlas {"), std::string::npos);
    EXPECT_NE(output.find("struct GameplayClient {"), std::string::npos);

    EXPECT_NE(output.find("runtime::PropertyStore<movement::Position> movement_position_store;"),
              std::string::npos);
    EXPECT_NE(output.find("runtime::PropertyStore<movement::MovementSpeed> movement_movement_speed_store;"),
              std::string::npos);

    EXPECT_NE(output.find("#include \"movement.capability.hpp\""), std::string::npos);

    EXPECT_NE(output.find("inline void register_property_stores(Context& ctx, GameplayClient& host)"),
              std::string::npos);
    EXPECT_NE(output.find("ctx.register_property_store(host.movement_position_store);"), std::string::npos);
    EXPECT_NE(output.find("ctx.register_property_store(host.movement_movement_speed_store);"),
              std::string::npos);
}

TEST(GenerateHostComposition, SkipsTheIncludeForACapabilityWithNoProperties) {
    const HostComposition composition{
        .host_name = "Minimal",
        .ordered_capabilities =
            {
                Manifest{
                    .capability_name = "interruption",
                    .depends_on = {"entity"},
                    .consumes = {},
                    .properties = {},
                    .requests = {},
                    .events = {StructDecl{.name = "ActionInterrupted",
                                          .fields = {Field{.name = "entity", .type = "EntityRef"}},
                                          .composition = std::nullopt}},
                },
            },
    };

    const std::string output =
        generate_host_composition(composition, "minimal.host.hpp", "minimal.host.yaml");

    EXPECT_EQ(output.find("#include \"interruption.capability.hpp\""), std::string::npos);
    EXPECT_NE(output.find("struct Minimal {\n"), std::string::npos);
}

TEST(GenerateHostComposition, TriggeredPropertyRegistersThroughRegisterTriggeredPropertyStore) {
    // A triggered property (spec §20, Triggered composition; issue #47) needs
    // its store reachable through Context::end_tick()'s tick-boundary reset
    // (issue #39), not just get<T>()/set<T>() - register_property_store<T>()
    // alone would never wire that up.
    const HostComposition composition{
        .host_name = "GameplayClient",
        .ordered_capabilities =
            {
                Manifest{
                    .capability_name = "movement",
                    .depends_on = {"entity"},
                    .consumes = {},
                    .properties = {StructDecl{.name = "Position",
                                              .fields = {Field{.name = "x", .type = "float"}},
                                              .composition = std::nullopt,
                                              .trigger = false},
                                   StructDecl{.name = "PositionChanged",
                                              .fields = {Field{.name = "new_x", .type = "float"}},
                                              .composition = std::nullopt,
                                              .trigger = true}},
                    .requests = {},
                    .events = {},
                },
            },
    };

    const std::string output =
        generate_host_composition(composition, "gameplay_client.host.hpp", "gameplay_client.host.yaml");

    EXPECT_NE(output.find("ctx.register_property_store(host.movement_position_store);"), std::string::npos);
    EXPECT_NE(output.find("ctx.register_triggered_property_store(host.movement_position_changed_store);"),
              std::string::npos);
    // Never both for the same property.
    EXPECT_EQ(output.find("ctx.register_property_store(host.movement_position_changed_store);"),
              std::string::npos);
}

TEST(GenerateHostComposition, MembersFollowCompositionOrder) {
    const HostComposition composition{
        .host_name = "GameplayClient",
        .ordered_capabilities =
            {
                Manifest{
                    .capability_name = "movement",
                    .depends_on = {},
                    .consumes = {},
                    .properties = {StructDecl{.name = "Position",
                                              .fields = {Field{.name = "x", .type = "float"}},
                                              .composition = std::nullopt}},
                    .requests = {},
                    .events = {},
                },
                Manifest{
                    .capability_name = "haste",
                    .depends_on = {"movement"},
                    .consumes = {},
                    .properties = {StructDecl{.name = "CastSpeed",
                                              .fields = {Field{.name = "base", .type = "float"}},
                                              .composition = "Multiplicative"}},
                    .requests = {},
                    .events = {},
                },
            },
    };

    const std::string output =
        generate_host_composition(composition, "gameplay_client.host.hpp", "gameplay_client.host.yaml");

    const auto movement_pos = output.find("movement_position_store");
    const auto haste_pos = output.find("haste_cast_speed_store");
    ASSERT_NE(movement_pos, std::string::npos);
    ASSERT_NE(haste_pos, std::string::npos);
    EXPECT_LT(movement_pos, haste_pos);
}

} // namespace
} // namespace atlas::cgen
