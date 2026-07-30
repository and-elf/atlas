// Fireball, worked end-to-end: proof that adding a new spell does not mean
// adding a new capability. Every number below (damage, cast time, range,
// animation, and the burn's own damage-per-tick/interval/duration) is a
// plain authored constant fed into requests two *already-existing*
// capabilities declare - cast_time_attack (the cast-bar mechanism) and
// damage_over_time (the recurring-damage mechanism). Fireball itself has
// no manifest, no generated contract, and no PropertyStore of its own -
// see demo/README.md's "Fireball: content is data, not a capability"
// section for the full argument this test exists to prove.
//
// The one piece of real code this "spell" needs - deciding that landing a
// Fireball *also* starts a burn, and that the burn's damage-per-tick is
// 20% of whatever the direct hit rolled - is exactly the kind of small,
// hand-written glue spec §14's Declarative Boundary says can never be pure
// YAML (it's a decision, not a structure). It's a few lines
// (apply_fireball_burn below), subscribed to cast_time_attack's own
// already-generic CastLanded event, the same host-composition-wiring shape
// demo/tests/simulated_host.hpp already uses for cancellation - not a new
// capability.
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"

#include <cstdint>
#include <gtest/gtest.h>

#include "cast_time_attack/cast_time_attack.hpp"
#include "damage_over_time/damage_over_time.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

// Fireball's own numbers - the only thing a "new spell" ever actually is:
// data fed into requests two existing capabilities already declare.
constexpr std::int32_t fireball_damage = 100;
constexpr std::uint64_t fireball_cast_time_ticks = 60;           // a 1-second cast bar
constexpr std::uint64_t fireball_burn_tick_interval_ticks = 180; // 3 seconds
constexpr std::uint64_t fireball_burn_applications = 3;          // 9 seconds total

// The entire cost of "Fireball also burns": landing one starts a DoT worth
// 20% of the direct hit, three times, three seconds apart. Extracted to a
// named function (rather than an inline subscription lambda) purely to
// keep the test body's own cognitive complexity under clang-tidy's
// threshold - the shape (a handful of lines reacting to an already-generic
// event) is the point, not where exactly it's spelled.
void apply_fireball_burn(Context& ctx, const cast_time_attack::CastLanded& landed) {
    request::Dispatcher<damage_over_time::ApplyDotEffect> apply_dot;
    apply_dot.register_handler(damage_over_time::on_apply_dot_effect);
    ASSERT_TRUE(apply_dot
                    .dispatch(ctx,
                              damage_over_time::ApplyDotEffect{
                                  .target = landed.target,
                                  .damage_per_tick = landed.damage / 5, // 20%
                                  .tick_interval_ticks = fireball_burn_tick_interval_ticks,
                                  .total_applications = fireball_burn_applications})
                    .accepted);
}

request::Dispatcher<cast_time_attack::BeginCast>
make_begin_cast_dispatcher(cast_time_attack::ActionRegistry& registry) {
    request::Dispatcher<cast_time_attack::BeginCast> dispatcher;
    dispatcher.register_handler([&registry](Context& ctx, const cast_time_attack::BeginCast& cmd) {
        return cast_time_attack::on_begin_cast(ctx, registry, cmd);
    });
    return dispatcher;
}

request::Dispatcher<cast_time_attack::AdvanceCast>
make_advance_cast_dispatcher(cast_time_attack::ActionRegistry& registry) {
    request::Dispatcher<cast_time_attack::AdvanceCast> dispatcher;
    dispatcher.register_handler([&registry](Context& ctx, const cast_time_attack::AdvanceCast& cmd) {
        return cast_time_attack::on_advance_cast(ctx, registry, cmd);
    });
    return dispatcher;
}

// Drives every one of the burn's own applications - extracted (rather than
// a loop inline in the test body) purely to keep TestBody's own cognitive
// complexity under clang-tidy's threshold.
void advance_dot_effect_repeatedly(Context& ctx,
                                   std::uint64_t delta_ticks,
                                   EntityRef target,
                                   std::uint64_t times) {
    request::Dispatcher<damage_over_time::AdvanceDotEffect> advance_dot;
    advance_dot.register_handler(damage_over_time::on_advance_dot_effect);
    for (std::uint64_t application = 0; application < times; ++application) {
        ASSERT_TRUE(
            advance_dot
                .dispatch(ctx,
                          damage_over_time::AdvanceDotEffect{.target = target, .delta_ticks = delta_ticks})
                .accepted);
    }
}

TEST(Fireball, DirectHitPlusBurnDealsTheFullExpectedTotal) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef caster = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    server.position_store.set(caster, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 0.0F});
    server.health_store.set(target, health::Health{.current = 200, .maximum = 200});
    server.cast_time_attack_store.set(caster, cast_time_attack::CastTimeAttack{});
    server.dot_effect_store.set(target, damage_over_time::DotEffect{});

    server.ctx.subscribe<cast_time_attack::CastLanded>(
        [&](const cast_time_attack::CastLanded& landed) { apply_fireball_burn(server.ctx, landed); });

    const ResourceId fireball_cast_animation = ResourceId::from_name("animations/fireball_cast");
    request::Dispatcher<cast_time_attack::BeginCast> begin_dispatcher =
        make_begin_cast_dispatcher(server.cast_action_registry);

    ASSERT_TRUE(begin_dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::BeginCast{.caster = caster,
                                                          .target = target,
                                                          .obstacle = EntityRef{},
                                                          .min_range = 0,
                                                          .max_range = 20,
                                                          .damage = fireball_damage,
                                                          .cast_time_ticks = fireball_cast_time_ticks,
                                                          .requires_stationary = true,
                                                          .animation = fireball_cast_animation})
                    .accepted);

    // Land the cast - the direct hit, and (via apply_fireball_burn above)
    // the burn's own ApplyDotEffect.
    request::Dispatcher<cast_time_attack::AdvanceCast> advance_dispatcher =
        make_advance_cast_dispatcher(server.cast_action_registry);
    ASSERT_TRUE(advance_dispatcher
                    .dispatch(server.ctx,
                              cast_time_attack::AdvanceCast{.caster = caster,
                                                            .delta_ticks = fireball_cast_time_ticks})
                    .accepted);
    ASSERT_EQ(server.ctx.get<health::Health>(target)->get().current, 100); // 200 - 100 direct hit

    advance_dot_effect_repeatedly(
        server.ctx, fireball_burn_tick_interval_ticks, target, fireball_burn_applications);

    // 200 (start) - 100 (direct hit) - 3 x 20 (20% burn, three times) == 40.
    EXPECT_EQ(server.ctx.get<health::Health>(target)->get().current, 40);
    EXPECT_EQ(server.ctx.get<damage_over_time::DotEffect>(target)->get().remaining_applications, 0);
}

} // namespace
} // namespace atlas::demo
