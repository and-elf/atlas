// Proves EquipArmor contributes to Armor through the same request-dispatch
// path as combat_scenario_test.cpp's ApplyDamage, and that a subsequent
// attack sees the mitigation - i.e. equipping is no longer test-harness-
// injected state (armor::add_contribution called directly in test setup,
// as combat_scenario_test.cpp still does for its own simpler scenarios) but
// a real request flowing through equipment's own capability, contributing
// to armor via the sanctioned channel (spec §20, Design Rule).
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"

#include <gtest/gtest.h>

#include "equipment/equipment.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(Equipment, EquipArmorContributesToArmorComposition) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.armor_store.set(target, armor::Armor{.base = 0});

    request::Dispatcher<equipment::EquipArmor> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const equipment::EquipArmor& cmd) {
        return equipment::on_equip_armor(ctx, server.armor_contributions, cmd);
    });

    const RequestResult result = dispatcher.dispatch(
        server.ctx,
        equipment::EquipArmor{.target = target, .item = ResourceId::from_name("plate_armor"), .bonus = 5});

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(server.ctx.get<armor::Armor>(target)->get().base, 5);
}

TEST(Equipment, EquipArmorRejectedWithoutAuthority) {
    SimulatedHost client{/*has_authority=*/false};
    const EntityRef target = client.host.create_entity();
    client.armor_store.set(target, armor::Armor{.base = 0});

    request::Dispatcher<equipment::EquipArmor> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const equipment::EquipArmor& cmd) {
        return equipment::on_equip_armor(ctx, client.armor_contributions, cmd);
    });

    const RequestResult result = dispatcher.dispatch(
        client.ctx,
        equipment::EquipArmor{.target = target, .item = ResourceId::from_name("plate_armor"), .bonus = 5});

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

TEST(Equipment, EquippingTwoItemsAccumulatesAdditively) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.armor_store.set(target, armor::Armor{.base = 0});

    request::Dispatcher<equipment::EquipArmor> dispatcher;
    dispatcher.register_handler([&](Context& ctx, const equipment::EquipArmor& cmd) {
        return equipment::on_equip_armor(ctx, server.armor_contributions, cmd);
    });

    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              equipment::EquipArmor{
                                  .target = target, .item = ResourceId::from_name("helmet"), .bonus = 2})
                    .accepted);
    ASSERT_TRUE(dispatcher
                    .dispatch(server.ctx,
                              equipment::EquipArmor{
                                  .target = target, .item = ResourceId::from_name("chestplate"), .bonus = 3})
                    .accepted);

    EXPECT_EQ(server.ctx.get<armor::Armor>(target)->get().base, 5);
}

TEST(Equipment, EquippingThenAttackingAppliesTheEquippedMitigation) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef target = server.host.create_entity();
    server.health_store.set(target, health::Health{.current = 10, .maximum = 10});
    server.armor_store.set(target, armor::Armor{.base = 0});

    request::Dispatcher<equipment::EquipArmor> equip_dispatcher;
    equip_dispatcher.register_handler([&](Context& ctx, const equipment::EquipArmor& cmd) {
        return equipment::on_equip_armor(ctx, server.armor_contributions, cmd);
    });
    request::Dispatcher<health::ApplyDamage> damage_dispatcher;
    damage_dispatcher.register_handler(health::on_apply_damage);

    ASSERT_TRUE(equip_dispatcher
                    .dispatch(server.ctx,
                              equipment::EquipArmor{
                                  .target = target, .item = ResourceId::from_name("plate_armor"), .bonus = 5})
                    .accepted);

    const RequestResult result =
        damage_dispatcher.dispatch(server.ctx, health::ApplyDamage{.target = target, .amount = 10});

    ASSERT_TRUE(result.accepted);
    // 10 damage, 5 equipped armor: 5 dealt - the same arithmetic
    // combat_scenario_test.cpp proves, but the mitigation now comes from a
    // real EquipArmor request rather than test-harness-injected state.
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 5);
}

} // namespace
} // namespace atlas::demo
