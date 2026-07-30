// Direct unit tests for armor's own logic (contribution registration and
// resolution), as opposed to combat_scenario_test.cpp/equipment_test.cpp's
// end-to-end scenario coverage.
#include <gtest/gtest.h>
#include <stdexcept>

#include "armor/armor.hpp"
#include "simulated_host.hpp"

namespace atlas::armor {
namespace {

using demo::testing::SimulatedHost;

TEST(AddContribution, ThrowsWhenTargetHasNoArmorPropertySeeded) {
    SimulatedHost host{/*has_authority=*/true};
    const EntityRef target = host.host.create_entity(); // no Armor seeded

    EXPECT_THROW((void)add_contribution(host.ctx, host.armor_contributions, target, "plate", 5),
                 std::logic_error);
}

TEST(AddContribution, ResolvesToBasePlusTheSingleContribution) {
    SimulatedHost host{/*has_authority=*/true};
    const EntityRef target = host.host.create_entity();
    host.armor_store.set(target, Armor{.base = 0});

    add_contribution(host.ctx, host.armor_contributions, target, "plate", 5);

    EXPECT_EQ(host.ctx.get<Armor>(target)->get().base, 5);
}

} // namespace
} // namespace atlas::armor
